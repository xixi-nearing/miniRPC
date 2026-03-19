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

  void AcceptLoop();
  void HandleClient(int client_fd, std::string peer_ip);
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
