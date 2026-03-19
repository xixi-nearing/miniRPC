#pragma once

#include <string>

#include "calculator_rpc.h"

namespace demo {

class DemoCalculatorService : public CalculatorService {
 public:
  mrpc::RpcStatus Add(const AddRequest& request, AddResponse* response) override;
  mrpc::RpcStatus Fib(const FibRequest& request, FibResponse* response) override;
  mrpc::RpcStatus Echo(const EchoRequest& request, EchoResponse* response) override;
  mrpc::RpcStatus Delay(const DelayRequest& request, DelayResponse* response) override;

  static std::uint64_t FibIterative(std::uint32_t n);
};

}  // namespace demo
