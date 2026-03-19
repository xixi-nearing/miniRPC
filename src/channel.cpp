#include "minirpc/channel.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <string>

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

}  // namespace

RpcChannel::RpcChannel(Endpoint endpoint, std::string auth_token)
    : endpoint_(std::move(endpoint)), auth_token_(std::move(auth_token)) {}

RpcChannel::~RpcChannel() {
  Reset();
}

RpcStatus RpcChannel::Call(const std::string& service,
                           const std::string& method,
                           const std::string& request_payload,
                           std::string* response_payload,
                           std::uint32_t timeout_ms) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto status = EnsureConnected();
  if (!status.ok()) {
    return status;
  }
  status = ApplySocketTimeout(timeout_ms);
  if (!status.ok()) {
    return status;
  }

  RequestFrame request;
  request.service = service;
  request.method = method;
  request.auth_token = auth_token_;
  request.deadline_unix_ms = NowUnixMillis() + timeout_ms;
  request.payload = request_payload;

  const auto body = EncodeRequestBody(request);
  FrameHeader header;
  header.message_type = MessageType::kRequest;
  header.request_id = next_request_id_++;
  header.body_length = static_cast<std::uint32_t>(body.size());

  const auto header_bytes = EncodeHeader(header);
  std::string outbound = header_bytes;
  outbound += body;
  if (!WriteFully(socket_fd_, outbound.data(), outbound.size())) {
    ResetUnlocked();
    return RpcStatus::Error(StatusCode::kNetworkError, "failed to send request");
  }

  std::string response_header_bytes(kFrameHeaderSize, '\0');
  if (!ReadFully(socket_fd_, response_header_bytes.data(), response_header_bytes.size())) {
    const auto code = (errno == EAGAIN || errno == EWOULDBLOCK) ? StatusCode::kTimeout
                                                                : StatusCode::kNetworkError;
    ResetUnlocked();
    return RpcStatus::Error(code, "failed to read response header");
  }

  FrameHeader response_header;
  status = DecodeHeader(response_header_bytes, &response_header);
  if (!status.ok()) {
    ResetUnlocked();
    return status;
  }
  if (response_header.message_type != MessageType::kResponse ||
      response_header.request_id != header.request_id) {
    ResetUnlocked();
    return RpcStatus::Error(StatusCode::kBadRequest, "unexpected response frame");
  }

  std::string response_body(response_header.body_length, '\0');
  if (!ReadFully(socket_fd_, response_body.data(), response_body.size())) {
    const auto code = (errno == EAGAIN || errno == EWOULDBLOCK) ? StatusCode::kTimeout
                                                                : StatusCode::kNetworkError;
    ResetUnlocked();
    return RpcStatus::Error(code, "failed to read response body");
  }

  ResponseFrame response;
  status = DecodeResponseBody(response_body, &response);
  if (!status.ok()) {
    Reset();
    return status;
  }
  if (response.status != StatusCode::kOk) {
    return RpcStatus::Error(response.status, response.message);
  }
  *response_payload = std::move(response.payload);
  return RpcStatus::Ok();
}

void RpcChannel::Reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  ResetUnlocked();
}

void RpcChannel::ResetUnlocked() {
  if (socket_fd_ >= 0) {
    ::close(socket_fd_);
    socket_fd_ = -1;
  }
}

RpcStatus RpcChannel::EnsureConnected() {
  if (socket_fd_ >= 0) {
    return RpcStatus::Ok();
  }

  socket_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd_ < 0) {
    return RpcStatus::Error(StatusCode::kNetworkError, "failed to create socket");
  }

  sockaddr_in address {};
  address.sin_family = AF_INET;
  address.sin_port = htons(endpoint_.port);
  if (::inet_pton(AF_INET, endpoint_.host.c_str(), &address.sin_addr) <= 0) {
    ::close(socket_fd_);
    socket_fd_ = -1;
    return RpcStatus::Error(StatusCode::kNetworkError, "invalid endpoint host");
  }

  if (::connect(socket_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
    ::close(socket_fd_);
    socket_fd_ = -1;
    return RpcStatus::Error(StatusCode::kNetworkError, "failed to connect");
  }

  int no_delay = 1;
  ::setsockopt(socket_fd_, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(no_delay));
  return RpcStatus::Ok();
}

RpcStatus RpcChannel::ApplySocketTimeout(std::uint32_t timeout_ms) {
  timeval tv {};
  tv.tv_sec = static_cast<long>(timeout_ms / 1000);
  tv.tv_usec = static_cast<long>((timeout_ms % 1000) * 1000);
  if (::setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0 ||
      ::setsockopt(socket_fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
    return RpcStatus::Error(StatusCode::kNetworkError, "failed to apply socket timeout");
  }
  return RpcStatus::Ok();
}

}  // namespace mrpc
