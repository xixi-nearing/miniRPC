#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

#include "calculator_service_impl.h"
#include "minirpc/channel.h"
#include "minirpc/server.h"

int main() {
  mrpc::ServerOptions options;
  options.host = "127.0.0.1";
  options.port = 0;
  options.worker_threads = 4;
  options.max_queue_size = 64;
  options.auth_token = "mini-secret";

  mrpc::RpcServer server(options);
  demo::DemoCalculatorService service;
  auto status = demo::RegisterCalculatorService(&server, &service);
  assert(status.ok());

  status = server.Start();
  assert(status.ok());
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  mrpc::Endpoint endpoint{"127.0.0.1", server.bound_port()};

  mrpc::RpcChannel good_channel(endpoint, "mini-secret");
  demo::CalculatorServiceClient good_client(&good_channel);

  demo::AddRequest add_request;
  add_request.lhs = 7;
  add_request.rhs = 35;
  demo::AddResponse add_response;
  status = good_client.Add(add_request, &add_response, 1000);
  assert(status.ok());
  assert(add_response.result == 42);
  for (int i = 0; i < 100; ++i) {
    add_request.lhs = i;
    add_request.rhs = i * 2;
    status = good_client.Add(add_request, &add_response, 1000);
    assert(status.ok());
    assert(add_response.result == add_request.lhs + add_request.rhs);
  }

  mrpc::RpcChannel bad_channel(endpoint, "wrong-secret");
  demo::CalculatorServiceClient bad_client(&bad_channel);
  demo::AddResponse denied_response;
  status = bad_client.Add(add_request, &denied_response, 1000);
  assert(!status.ok());
  assert(status.code() == mrpc::StatusCode::kAuthFailed);

  demo::DelayRequest delay_request;
  delay_request.sleep_ms = 50;
  demo::DelayResponse delay_response;
  status = good_client.Delay(delay_request, &delay_response, 5);
  assert(!status.ok());
  assert(status.code() == mrpc::StatusCode::kTimeout);

  good_channel.Reset();
  bad_channel.Reset();
  server.Stop();
  std::cout << "test_rpc_integration passed\n";
  return 0;
}
