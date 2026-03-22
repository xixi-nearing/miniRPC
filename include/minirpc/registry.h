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
  // 读取静态配置文件，并缓存客户端默认鉴权 token / 超时参数。
  RpcStatus LoadFromFile(const std::string& path);
  // 按服务名查询单个固定地址；后续如果替换成 etcd 等动态注册中心，可保留该抽象。
  std::optional<Endpoint> Find(const std::string& service_name) const;
  std::string auth_token() const { return auth_token_; }
  std::uint32_t timeout_ms() const { return timeout_ms_; }

 private:
  IniConfig config_;
  std::string auth_token_;
  std::uint32_t timeout_ms_ = 1000;
};

}  // namespace mrpc
