#include "minirpc/server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <future>
#include <memory>
#include <utility>

namespace mrpc {

namespace {

bool WriteFully(int fd, const char* data, std::size_t size) {
  std::size_t written = 0;
  while (written < size) {
    const auto rc = ::send(fd, data + written, size - written, MSG_NOSIGNAL);
    if (rc <= 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    written += static_cast<std::size_t>(rc);
  }
  return true;
}

bool ReadFully(int fd, char* data, std::size_t size) {
  std::size_t read_bytes = 0;
  while (read_bytes < size) {
    const auto rc = ::recv(fd, data + read_bytes, size - read_bytes, 0);
    if (rc == 0) {
      return false;
    }
    if (rc < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    read_bytes += static_cast<std::size_t>(rc);
  }
  return true;
}

std::string ResolvePeerIp(const sockaddr_in& address) {
  char buffer[INET_ADDRSTRLEN] = {0};
  if (::inet_ntop(AF_INET, &address.sin_addr, buffer, sizeof(buffer)) == nullptr) {
    return "";
  }
  return std::string(buffer);
}

}  // namespace

RpcServer::RpcServer(ServerOptions options)
    : options_(std::move(options)),
      executor_(options_.worker_threads, options_.max_queue_size) {}

RpcServer::~RpcServer() {
  Stop();
}

void RpcServer::RegisterMethod(const std::string& service,
                               const std::string& method,
                               MethodHandler handler) {
  handlers_[service][method] = std::move(handler);
}

RpcStatus RpcServer::Start() {
  if (running_.exchange(true)) {
    return RpcStatus::Error(StatusCode::kInternalError, "server already running");
  }

  listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) {
    running_ = false;
    return RpcStatus::Error(StatusCode::kNetworkError, "failed to create server socket");
  }

  int reuse_addr = 1;
  ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));

  sockaddr_in address {};
  address.sin_family = AF_INET;
  address.sin_port = htons(options_.port);
  if (::inet_pton(AF_INET, options_.host.c_str(), &address.sin_addr) <= 0) {
    ::close(listen_fd_);
    listen_fd_ = -1;
    running_ = false;
    return RpcStatus::Error(StatusCode::kNetworkError, "invalid server host");
  }

  if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
    ::close(listen_fd_);
    listen_fd_ = -1;
    running_ = false;
    return RpcStatus::Error(StatusCode::kNetworkError, "failed to bind server socket");
  }
  if (::listen(listen_fd_, options_.listen_backlog) < 0) {
    ::close(listen_fd_);
    listen_fd_ = -1;
    running_ = false;
    return RpcStatus::Error(StatusCode::kNetworkError, "failed to listen on server socket");
  }

  sockaddr_in bound {};
  socklen_t bound_len = sizeof(bound);
  if (::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&bound), &bound_len) == 0) {
    bound_port_ = ntohs(bound.sin_port);
  }

  accept_thread_ = std::thread([this]() { AcceptLoop(); });
  return RpcStatus::Ok();
}

void RpcServer::Stop() {
  if (!running_.exchange(false)) {
    return;
  }
  if (listen_fd_ >= 0) {
    ::shutdown(listen_fd_, SHUT_RDWR);
    ::close(listen_fd_);
    listen_fd_ = -1;
  }
  if (accept_thread_.joinable()) {
    accept_thread_.join();
  }
  executor_.Stop();
}

void RpcServer::Wait() {
  if (accept_thread_.joinable()) {
    accept_thread_.join();
  }
}

void RpcServer::AcceptLoop() {
  while (running_) {
    sockaddr_in peer {};
    socklen_t peer_len = sizeof(peer);
    const auto client_fd =
        ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&peer), &peer_len);
    if (client_fd < 0) {
      if (!running_ || errno == EBADF || errno == EINVAL) {
        return;
      }
      if (errno == EINTR) {
        continue;
      }
      continue;
    }

    const auto peer_ip = ResolvePeerIp(peer);
    if (!IsAllowedPeer(peer_ip)) {
      ::close(client_fd);
      continue;
    }

    int no_delay = 1;
    ::setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(no_delay));

    // 连接读写线程与业务线程池分离：
    // 前者负责协议收发，后者负责真正执行 RPC 方法，避免业务阻塞 accept。
    std::thread([this, client_fd, peer_ip]() {
      HandleClient(client_fd, peer_ip);
    }).detach();
  }
}

void RpcServer::HandleClient(int client_fd, std::string peer_ip) {
  (void)peer_ip;
  while (running_) {
    std::string header_bytes(kFrameHeaderSize, '\0');
    if (!ReadFully(client_fd, header_bytes.data(), header_bytes.size())) {
      break;
    }

    FrameHeader header;
    auto status = DecodeHeader(header_bytes, &header);
    if (!status.ok() || header.message_type != MessageType::kRequest) {
      break;
    }

    std::string body(header.body_length, '\0');
    if (!ReadFully(client_fd, body.data(), body.size())) {
      break;
    }

    RequestFrame request;
    status = DecodeRequestBody(body, &request);
    ResponseFrame response;
    if (!status.ok()) {
      response.status = status.code();
      response.message = status.message();
    } else {
      auto promise = std::make_shared<std::promise<ResponseFrame>>();
      auto future = promise->get_future();
      // 连接线程只负责把请求投递到线程池，并同步等待结果，保证单连接上的响应顺序稳定。
      const auto scheduled =
          executor_.Submit([this, request, promise]() mutable { promise->set_value(Dispatch(request)); });
      if (!scheduled) {
        response.status = StatusCode::kBusy;
        response.message = "executor queue is full";
      } else {
        response = future.get();
      }
    }

    const auto response_body = EncodeResponseBody(response);
    FrameHeader response_header;
    response_header.message_type = MessageType::kResponse;
    response_header.request_id = header.request_id;
    response_header.body_length = static_cast<std::uint32_t>(response_body.size());

    const auto response_header_bytes = EncodeHeader(response_header);
    std::string outbound = response_header_bytes;
    outbound += response_body;
    if (!WriteFully(client_fd, outbound.data(), outbound.size())) {
      break;
    }
  }
  ::shutdown(client_fd, SHUT_RDWR);
  ::close(client_fd);
}

ResponseFrame RpcServer::Dispatch(const RequestFrame& request) const {
  ResponseFrame response;
  // 框架级校验统一放在这里，业务 handler 只关心 payload 处理。
  if (!options_.auth_token.empty() && request.auth_token != options_.auth_token) {
    response.status = StatusCode::kAuthFailed;
    response.message = "auth token mismatch";
    return response;
  }
  if (request.deadline_unix_ms != 0 && request.deadline_unix_ms < NowUnixMillis()) {
    response.status = StatusCode::kTimeout;
    response.message = "request deadline exceeded before execution";
    return response;
  }

  const auto service_it = handlers_.find(request.service);
  if (service_it == handlers_.end()) {
    response.status = StatusCode::kUnknownService;
    response.message = "unknown service: " + request.service;
    return response;
  }

  const auto method_it = service_it->second.find(request.method);
  if (method_it == service_it->second.end()) {
    response.status = StatusCode::kUnknownMethod;
    response.message = "unknown method: " + request.method;
    return response;
  }

  auto status = method_it->second(request.payload, &response.payload);
  response.status = status.code();
  response.message = status.message();
  return response;
}

bool RpcServer::IsAllowedPeer(const std::string& peer_ip) const {
  if (options_.ip_allowlist.empty()) {
    return true;
  }
  return options_.ip_allowlist.contains(peer_ip);
}

}  // namespace mrpc
