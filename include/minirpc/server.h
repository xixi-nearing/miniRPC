#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include "minirpc/protocol.h"
#include "minirpc/thread_pool.h"

namespace mrpc {

struct ServerOptions {
  std::string host = "127.0.0.1";
  std::uint16_t port = 0;
  std::size_t worker_threads = 4;
  std::size_t max_queue_size = 256;
  std::string auth_token;
  std::unordered_set<std::string> ip_allowlist;
  int listen_backlog = 64;
};

class RpcServer {
 public:
  // handler 接收序列化后的业务请求，输出序列化后的业务响应。
  // 这样运行时不依赖具体 IDL 类型，生成代码负责在两端做对象与字节串的转换。
  using MethodHandler = std::function<RpcStatus(const std::string&, std::string*)>;

  explicit RpcServer(ServerOptions options);
  ~RpcServer();

  void RegisterMethod(const std::string& service,
                      const std::string& method,
                      MethodHandler handler);

  RpcStatus Start();
  void Stop();
  void Wait();

  std::uint16_t bound_port() const { return bound_port_; }

 private:
  using MethodMap = std::unordered_map<std::string, MethodHandler>;

  // AcceptLoop 只负责接入连接；每个连接的收发循环在独立线程中运行。
  void AcceptLoop();
  void HandleClient(int client_fd, std::string peer_ip);
  // Dispatch 是“协议层 -> 业务层”的边界，完成鉴权、deadline 校验和方法路由。
  ResponseFrame Dispatch(const RequestFrame& request) const;
  bool IsAllowedPeer(const std::string& peer_ip) const;

  ServerOptions options_;
  ThreadPool executor_;
  std::unordered_map<std::string, MethodMap> handlers_;
  std::atomic<bool> running_{false};
  int listen_fd_ = -1;
  std::uint16_t bound_port_ = 0;
  std::thread accept_thread_;
};

}  // namespace mrpc
