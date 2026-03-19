#pragma once

#include <cstdint>
#include <string>

namespace mrpc {

enum class StatusCode : std::int32_t {
  kOk = 0,
  kUnknownService = 1001,
  kUnknownMethod = 1002,
  kBadRequest = 1003,
  kTimeout = 1004,
  kAuthFailed = 1005,
  kBusy = 1006,
  kNetworkError = 1007,
  kSerializeError = 1008,
  kInternalError = 1009,
};

class RpcStatus {
 public:
  RpcStatus() = default;
  RpcStatus(StatusCode code, std::string message)
      : code_(code), message_(std::move(message)) {}

  static RpcStatus Ok() { return RpcStatus(); }

  static RpcStatus Error(StatusCode code, std::string message) {
    return RpcStatus(code, std::move(message));
  }

  bool ok() const { return code_ == StatusCode::kOk; }
  StatusCode code() const { return code_; }
  const std::string& message() const { return message_; }

 private:
  StatusCode code_ = StatusCode::kOk;
  std::string message_;
};

}  // namespace mrpc
