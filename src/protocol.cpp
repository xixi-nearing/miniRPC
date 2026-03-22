#include "minirpc/protocol.h"

#include <chrono>

#include "minirpc/buffer.h"

namespace mrpc {

std::string EncodeHeader(const FrameHeader& header) {
  BufferWriter writer;
  // 编码顺序必须与 DecodeHeader 完全一致；header 大小固定，便于直接按字节数读取。
  writer.WriteIntegral(header.magic);
  writer.WriteIntegral(header.version);
  writer.WriteIntegral(static_cast<std::uint8_t>(header.message_type));
  writer.WriteIntegral(static_cast<std::uint8_t>(header.codec));
  writer.WriteIntegral(header.flags);
  writer.WriteIntegral(header.request_id);
  writer.WriteIntegral(header.body_length);
  return writer.Take();
}

RpcStatus DecodeHeader(std::string_view data, FrameHeader* header) {
  if (data.size() != kFrameHeaderSize) {
    return RpcStatus::Error(StatusCode::kBadRequest, "invalid frame header size");
  }

  BufferReader reader(data);
  std::uint8_t message_type = 0;
  std::uint8_t codec = 0;

  if (!reader.ReadIntegral(&header->magic) ||
      !reader.ReadIntegral(&header->version) ||
      !reader.ReadIntegral(&message_type) ||
      !reader.ReadIntegral(&codec) ||
      !reader.ReadIntegral(&header->flags) ||
      !reader.ReadIntegral(&header->request_id) ||
      !reader.ReadIntegral(&header->body_length)) {
    return RpcStatus::Error(StatusCode::kBadRequest, "failed to decode frame header");
  }

  header->message_type = static_cast<MessageType>(message_type);
  header->codec = static_cast<CodecType>(codec);

  // 先校验协议身份和版本，再继续读 body，避免把无效流量交给上层。
  if (header->magic != kProtocolMagic) {
    return RpcStatus::Error(StatusCode::kBadRequest, "protocol magic mismatch");
  }
  if (header->version != kProtocolVersion) {
    return RpcStatus::Error(StatusCode::kBadRequest, "protocol version mismatch");
  }
  return RpcStatus::Ok();
}

std::string EncodeRequestBody(const RequestFrame& request) {
  BufferWriter writer;
  writer.WriteString(request.service);
  writer.WriteString(request.method);
  writer.WriteString(request.auth_token);
  writer.WriteIntegral(request.deadline_unix_ms);
  writer.WriteString(request.payload);
  return writer.Take();
}

RpcStatus DecodeRequestBody(std::string_view data, RequestFrame* request) {
  BufferReader reader(data);
  if (!reader.ReadString(&request->service) ||
      !reader.ReadString(&request->method) ||
      !reader.ReadString(&request->auth_token) ||
      !reader.ReadIntegral(&request->deadline_unix_ms) ||
      !reader.ReadString(&request->payload) ||
      // body 必须恰好被消费完，防止“前缀合法 + 尾部脏数据”悄悄混入协议流。
      !reader.empty()) {
    return RpcStatus::Error(StatusCode::kBadRequest, "failed to decode request body");
  }
  return RpcStatus::Ok();
}

std::string EncodeResponseBody(const ResponseFrame& response) {
  BufferWriter writer;
  writer.WriteIntegral(static_cast<std::int32_t>(response.status));
  writer.WriteString(response.message);
  writer.WriteString(response.payload);
  return writer.Take();
}

RpcStatus DecodeResponseBody(std::string_view data, ResponseFrame* response) {
  BufferReader reader(data);
  std::int32_t status_code = 0;
  if (!reader.ReadIntegral(&status_code) ||
      !reader.ReadString(&response->message) ||
      !reader.ReadString(&response->payload) ||
      !reader.empty()) {
    return RpcStatus::Error(StatusCode::kBadRequest, "failed to decode response body");
  }
  // 响应状态码在线上格式里是 int32，解码后再映射回框架枚举。
  response->status = static_cast<StatusCode>(status_code);
  return RpcStatus::Ok();
}

std::uint64_t NowUnixMillis() {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

}  // namespace mrpc
