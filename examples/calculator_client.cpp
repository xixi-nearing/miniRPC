#include <iostream>
#include <string>

#include "calculator_service_impl.h"
#include "minirpc/registry.h"

int main(int argc, char** argv) {
  const std::string registry_path = argc > 1 ? argv[1] : "configs/registry.ini";

  mrpc::StaticRegistry registry;
  auto status = registry.LoadFromFile(registry_path);
  if (!status.ok()) {
    std::cerr << "load registry failed: " << status.message() << '\n';
    return 1;
  }

  auto endpoint = registry.Find("CalculatorService");
  if (!endpoint.has_value()) {
    std::cerr << "service not found in registry\n";
    return 1;
  }

  // 客户端 Stub 内部会把强类型请求对象序列化，然后经 RpcChannel 发到服务端。
  mrpc::RpcChannel channel(*endpoint, registry.auth_token());
  demo::CalculatorServiceClient client(&channel);

  demo::AddRequest add_request;
  add_request.lhs = 12;
  add_request.rhs = 30;
  demo::AddResponse add_response;
  status = client.Add(add_request, &add_response, registry.timeout_ms());
  if (!status.ok()) {
    std::cerr << "Add failed: " << status.message() << '\n';
    return 1;
  }
  std::cout << "Add(12, 30) = " << add_response.result << '\n';

  demo::FibRequest fib_request;
  fib_request.n = 20;
  demo::FibResponse fib_response;
  status = client.Fib(fib_request, &fib_response, registry.timeout_ms());
  if (!status.ok()) {
    std::cerr << "Fib failed: " << status.message() << '\n';
    return 1;
  }
  std::cout << "Fib(20) = " << fib_response.value << '\n';

  demo::EchoRequest echo_request;
  echo_request.payload = "hello from MiniRPC";
  demo::EchoResponse echo_response;
  status = client.Echo(echo_request, &echo_response, registry.timeout_ms());
  if (!status.ok()) {
    std::cerr << "Echo failed: " << status.message() << '\n';
    return 1;
  }
  std::cout << "Echo = " << echo_response.payload << '\n';
  return 0;
}
