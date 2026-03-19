#include "minirpc/registry.h"

namespace mrpc {

RpcStatus StaticRegistry::LoadFromFile(const std::string& path) {
  auto status = config_.LoadFromFile(path);
  if (!status.ok()) {
    return status;
  }

  auth_token_ = config_.GetString("client", "auth_token", "");
  timeout_ms_ = static_cast<std::uint32_t>(config_.GetInt("client", "timeout_ms", 1000));
  return RpcStatus::Ok();
}

std::optional<Endpoint> StaticRegistry::Find(const std::string& service_name) const {
  const auto host = config_.GetString(service_name, "host", "");
  const auto port = config_.GetInt(service_name, "port", 0);
  if (host.empty() || port <= 0 || port > 65535) {
    return std::nullopt;
  }
  return Endpoint{host, static_cast<std::uint16_t>(port)};
}

}  // namespace mrpc
