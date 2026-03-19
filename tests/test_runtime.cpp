#include <cassert>
#include <filesystem>
#include <iostream>

#include "minirpc/config.h"
#include "minirpc/protocol.h"
#include "minirpc/registry.h"

namespace {

std::filesystem::path ProjectRoot() {
  return std::filesystem::path(__FILE__).parent_path().parent_path();
}

void TestHeaderRoundTrip() {
  mrpc::FrameHeader header;
  header.message_type = mrpc::MessageType::kResponse;
  header.request_id = 42;
  header.body_length = 128;

  const auto data = mrpc::EncodeHeader(header);
  assert(data.size() == mrpc::kFrameHeaderSize);

  mrpc::FrameHeader decoded;
  const auto status = mrpc::DecodeHeader(data, &decoded);
  assert(status.ok());
  assert(decoded.magic == mrpc::kProtocolMagic);
  assert(decoded.message_type == mrpc::MessageType::kResponse);
  assert(decoded.request_id == 42);
  assert(decoded.body_length == 128);
}

void TestRequestResponseRoundTrip() {
  mrpc::RequestFrame request;
  request.service = "CalculatorService";
  request.method = "Add";
  request.auth_token = "mini-secret";
  request.deadline_unix_ms = 123456;
  request.payload = "payload";

  mrpc::RequestFrame decoded_request;
  auto status = mrpc::DecodeRequestBody(mrpc::EncodeRequestBody(request), &decoded_request);
  assert(status.ok());
  assert(decoded_request.service == request.service);
  assert(decoded_request.method == request.method);
  assert(decoded_request.auth_token == request.auth_token);
  assert(decoded_request.deadline_unix_ms == request.deadline_unix_ms);
  assert(decoded_request.payload == request.payload);

  mrpc::ResponseFrame response;
  response.status = mrpc::StatusCode::kBusy;
  response.message = "queue full";
  response.payload = "oops";

  mrpc::ResponseFrame decoded_response;
  status = mrpc::DecodeResponseBody(mrpc::EncodeResponseBody(response), &decoded_response);
  assert(status.ok());
  assert(decoded_response.status == mrpc::StatusCode::kBusy);
  assert(decoded_response.message == response.message);
  assert(decoded_response.payload == response.payload);
}

void TestConfigAndRegistry() {
  mrpc::IniConfig config;
  auto status = config.LoadFromFile((ProjectRoot() / "configs/server.ini").string());
  assert(status.ok());
  assert(config.GetString("server", "host") == "127.0.0.1");
  assert(config.GetInt("server", "port") == 5050);
  const auto allowlist = config.GetList("server", "allowlist");
  assert(allowlist.size() == 2);

  mrpc::StaticRegistry registry;
  status = registry.LoadFromFile((ProjectRoot() / "configs/registry.ini").string());
  assert(status.ok());
  const auto endpoint = registry.Find("CalculatorService");
  assert(endpoint.has_value());
  assert(endpoint->host == "127.0.0.1");
  assert(endpoint->port == 5050);
  assert(registry.auth_token() == "mini-secret");
  assert(registry.timeout_ms() == 1000);
}

}  // namespace

int main() {
  TestHeaderRoundTrip();
  TestRequestResponseRoundTrip();
  TestConfigAndRegistry();
  std::cout << "test_runtime passed\n";
  return 0;
}
