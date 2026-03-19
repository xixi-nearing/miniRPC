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

  RpcStatus Call(const std::string& service,
                 const std::string& method,
                 const std::string& request_payload,
                 std::string* response_payload,
                 std::uint32_t timeout_ms = 1000);

  void Reset();

 private:
  RpcStatus EnsureConnected();
  RpcStatus ApplySocketTimeout(std::uint32_t timeout_ms);
  void ResetUnlocked();

  Endpoint endpoint_;
  std::string auth_token_;
  int socket_fd_ = -1;
  std::uint64_t next_request_id_ = 1;
  std::mutex mutex_;
};

}  // namespace mrpc
