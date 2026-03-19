#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "minirpc/status.h"

namespace mrpc {

class IniConfig {
 public:
  RpcStatus LoadFromFile(const std::string& path);

  std::string GetString(const std::string& section,
                        const std::string& key,
                        const std::string& fallback = "") const;
  std::int64_t GetInt(const std::string& section,
                      const std::string& key,
                      std::int64_t fallback = 0) const;
  std::vector<std::string> GetList(const std::string& section,
                                   const std::string& key) const;

 private:
  std::unordered_map<std::string, std::unordered_map<std::string, std::string>> data_;
};

}  // namespace mrpc
