#include "calculator_service_impl.h"

#include <chrono>
#include <thread>

namespace demo {

mrpc::RpcStatus DemoCalculatorService::Add(const AddRequest& request, AddResponse* response) {
  response->result = request.lhs + request.rhs;
  return mrpc::RpcStatus::Ok();
}

mrpc::RpcStatus DemoCalculatorService::Fib(const FibRequest& request, FibResponse* response) {
  response->value = FibIterative(request.n);
  return mrpc::RpcStatus::Ok();
}

mrpc::RpcStatus DemoCalculatorService::Echo(const EchoRequest& request, EchoResponse* response) {
  response->payload = request.payload;
  return mrpc::RpcStatus::Ok();
}

mrpc::RpcStatus DemoCalculatorService::Delay(const DelayRequest& request, DelayResponse* response) {
  std::this_thread::sleep_for(std::chrono::milliseconds(request.sleep_ms));
  response->state = "slept";
  return mrpc::RpcStatus::Ok();
}

std::uint64_t DemoCalculatorService::FibIterative(std::uint32_t n) {
  if (n < 2) {
    return n;
  }
  std::uint64_t a = 0;
  std::uint64_t b = 1;
  for (std::uint32_t i = 2; i <= n; ++i) {
    const auto next = a + b;
    a = b;
    b = next;
  }
  return b;
}

}  // namespace demo
