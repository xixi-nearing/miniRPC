#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "minirpc/status.h"

namespace mrpc {

// 固定帧头常量。先读 header 再读 body，可以让 TCP 字节流恢复出消息边界。
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
  // request_id 用于将响应和请求配对，当前同步模型下也保留该字段，便于后续扩展并发复用。
  std::uint64_t request_id = 0;
  // body_length 只描述 body 大小，不包含 header 自身。
  std::uint32_t body_length = 0;
};

struct RequestFrame {
  std::string service;
  std::string method;
  std::string auth_token;
  // deadline 由客户端写入，服务端在真正执行业务前做一次快速失败判断。
  std::uint64_t deadline_unix_ms = 0;
  std::string payload;
};

struct ResponseFrame {
  StatusCode status = StatusCode::kOk;
  std::string message;
  std::string payload;
};

// Header 采用固定字段顺序编码，便于服务端做最小代价的合法性校验。
std::string EncodeHeader(const FrameHeader& header);
RpcStatus DecodeHeader(std::string_view data, FrameHeader* header);

// 请求体 / 响应体继续沿用“按字段顺序写入”的简单二进制格式。
std::string EncodeRequestBody(const RequestFrame& request);
RpcStatus DecodeRequestBody(std::string_view data, RequestFrame* request);

std::string EncodeResponseBody(const ResponseFrame& response);
RpcStatus DecodeResponseBody(std::string_view data, ResponseFrame* response);

// deadline 与超时控制统一使用 Unix 毫秒时间戳。
std::uint64_t NowUnixMillis();

}  // namespace mrpc
