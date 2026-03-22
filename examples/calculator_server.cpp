#include <iostream>
#include <string>
#include <unordered_set>

#include "calculator_service_impl.h"
#include "minirpc/config.h"
#include "minirpc/server.h"

int main(int argc, char** argv) {
  const std::string config_path = argc > 1 ? argv[1] : "configs/server.ini";

  mrpc::IniConfig config;
  auto status = config.LoadFromFile(config_path);
  if (!status.ok()) {
    std::cerr << "load config failed: " << status.message() << '\n';
    return 1;
  }

  mrpc::ServerOptions options;
  options.host = config.GetString("server", "host", "127.0.0.1");
  options.port = static_cast<std::uint16_t>(config.GetInt("server", "port", 5050));
  options.worker_threads =
      static_cast<std::size_t>(config.GetInt("server", "worker_threads", 4));
  options.max_queue_size =
      static_cast<std::size_t>(config.GetInt("server", "max_queue_size", 256));
  options.auth_token = config.GetString("server", "auth_token", "");
  for (const auto& peer : config.GetList("server", "allowlist")) {
    options.ip_allowlist.insert(peer);
  }

  // 生成代码把强类型服务实现注册成运行时可识别的 service + method 处理器。
  mrpc::RpcServer server(options);
  demo::DemoCalculatorService service;
  status = demo::RegisterCalculatorService(&server, &service);
  if (!status.ok()) {
    std::cerr << "register service failed: " << status.message() << '\n';
    return 1;
  }

  status = server.Start();
  if (!status.ok()) {
    std::cerr << "start server failed: " << status.message() << '\n';
    return 1;
  }

  std::cout << "MiniRPC server is listening on " << options.host << ':'
            << server.bound_port() << '\n';
  // 示例程序保持前台阻塞，便于直接观察端到端调用。
  server.Wait();
  return 0;
}
