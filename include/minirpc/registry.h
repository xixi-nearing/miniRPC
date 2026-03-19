#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "minirpc/config.h"

namespace mrpc {

struct Endpoint {
  std::string host = "127.0.0.1";
  std::uint16_t port = 0;
};

class StaticRegistry {
 public:
  RpcStatus LoadFromFile(const std::string& path);
  std::optional<Endpoint> Find(const std::string& service_name) const;
  std::string auth_token() const { return auth_token_; }
  std::uint32_t timeout_ms() const { return timeout_ms_; }

 private:
  IniConfig config_;
  std::string auth_token_;
  std::uint32_t timeout_ms_ = 1000;
};

}  // namespace mrpc
