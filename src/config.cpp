#include "minirpc/config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace mrpc {

namespace {

std::string Trim(const std::string& input) {
  std::size_t start = 0;
  while (start < input.size() &&
         std::isspace(static_cast<unsigned char>(input[start])) != 0) {
    ++start;
  }

  std::size_t end = input.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(input[end - 1])) != 0) {
    --end;
  }
  return input.substr(start, end - start);
}

}  // namespace

RpcStatus IniConfig::LoadFromFile(const std::string& path) {
  std::ifstream input(path);
  if (!input.is_open()) {
    return RpcStatus::Error(StatusCode::kInternalError, "failed to open config: " + path);
  }

  data_.clear();
  std::string section = "default";
  std::string line;
  std::size_t line_no = 0;
  while (std::getline(input, line)) {
    ++line_no;
    const auto trimmed = Trim(line);
    if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
      continue;
    }
    if (trimmed.front() == '[' && trimmed.back() == ']') {
      section = Trim(trimmed.substr(1, trimmed.size() - 2));
      continue;
    }
    const auto pos = trimmed.find('=');
    if (pos == std::string::npos) {
      return RpcStatus::Error(
          StatusCode::kBadRequest,
          "invalid config line " + std::to_string(line_no) + " in " + path);
    }
    const auto key = Trim(trimmed.substr(0, pos));
    const auto value = Trim(trimmed.substr(pos + 1));
    data_[section][key] = value;
  }
  return RpcStatus::Ok();
}

std::string IniConfig::GetString(const std::string& section,
                                 const std::string& key,
                                 const std::string& fallback) const {
  const auto section_it = data_.find(section);
  if (section_it == data_.end()) {
    return fallback;
  }
  const auto value_it = section_it->second.find(key);
  if (value_it == section_it->second.end()) {
    return fallback;
  }
  return value_it->second;
}

std::int64_t IniConfig::GetInt(const std::string& section,
                               const std::string& key,
                               std::int64_t fallback) const {
  const auto value = GetString(section, key);
  if (value.empty()) {
    return fallback;
  }
  try {
    return std::stoll(value);
  } catch (...) {
    return fallback;
  }
}

std::vector<std::string> IniConfig::GetList(const std::string& section,
                                            const std::string& key) const {
  std::vector<std::string> items;
  std::stringstream stream(GetString(section, key));
  std::string item;
  while (std::getline(stream, item, ',')) {
    const auto trimmed = Trim(item);
    if (!trimmed.empty()) {
      items.push_back(trimmed);
    }
  }
  return items;
}

}  // namespace mrpc
