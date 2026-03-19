#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "minirpc/status.h"

namespace mrpc {

constexpr std::uint32_t kProtocolMagic = 0x4D525043;
constexpr std::uint8_t kProtocolVersion = 1;
constexpr std::size_t kFrameHeaderSize = 20;

enum class MessageType : std::uint8_t {
  kRequest = 1,
  kResponse = 2,
};

enum class CodecType : std::uint8_t {
  kMiniBinary = 1,
};

struct FrameHeader {
  std::uint32_t magic = kProtocolMagic;
  std::uint8_t version = kProtocolVersion;
  MessageType message_type = MessageType::kRequest;
  CodecType codec = CodecType::kMiniBinary;
  std::uint8_t flags = 0;
  std::uint64_t request_id = 0;
  std::uint32_t body_length = 0;
};

struct RequestFrame {
  std::string service;
  std::string method;
  std::string auth_token;
  std::uint64_t deadline_unix_ms = 0;
  std::string payload;
};

struct ResponseFrame {
  StatusCode status = StatusCode::kOk;
  std::string message;
  std::string payload;
};

std::string EncodeHeader(const FrameHeader& header);
RpcStatus DecodeHeader(std::string_view data, FrameHeader* header);

std::string EncodeRequestBody(const RequestFrame& request);
RpcStatus DecodeRequestBody(std::string_view data, RequestFrame* request);

std::string EncodeResponseBody(const ResponseFrame& response);
RpcStatus DecodeResponseBody(std::string_view data, ResponseFrame* response);

std::uint64_t NowUnixMillis();

}  // namespace mrpc
