#pragma once

#include <cstdint>
#include <mutex>
#include <string>

#include "minirpc/protocol.h"
#include "minirpc/registry.h"

namespace mrpc {

class RpcChannel {
 public:
  RpcChannel(Endpoint endpoint, std::string auth_token = "");
  ~RpcChannel();

  // 同步调用接口。
  // 一个 channel 复用一个 TCP 连接；mutex 保证同一时刻只有一个请求在该连接上飞行。
  RpcStatus Call(const std::string& service,
                 const std::string& method,
                 const std::string& request_payload,
                 std::string* response_payload,
                 std::uint32_t timeout_ms = 1000);

  void Reset();

 private:
  // 懒连接：第一次调用时建立连接，后续请求复用该 socket。
  RpcStatus EnsureConnected();
  // 为当前调用刷新读写超时，避免旧请求的超时设置影响新请求。
  RpcStatus ApplySocketTimeout(std::uint32_t timeout_ms);
  void ResetUnlocked();

  Endpoint endpoint_;
  std::string auth_token_;
  int socket_fd_ = -1;
  // 当前实现是串行同步调用，但仍保留 request_id 递增，便于未来支持连接内多路复用。
  std::uint64_t next_request_id_ = 1;
  std::mutex mutex_;
};

}  // namespace mrpc
