#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "calculator_service_impl.h"
#include "minirpc/channel.h"
#include "minirpc/server.h"

namespace {

using Clock = std::chrono::steady_clock;

struct ScenarioResult {
  std::string scenario;
  int concurrency = 0;
  int payload_bytes = 0;
  int total_ops = 0;
  int success = 0;
  int busy = 0;
  int timeout = 0;
  int failures = 0;
  double elapsed_ms = 0.0;
  double qps = 0.0;
  double avg_us = 0.0;
  double p50_us = 0.0;
  double p95_us = 0.0;
  double p99_us = 0.0;
};

struct ThreadStats {
  std::vector<double> latencies_us;
  int success = 0;
  int busy = 0;
  int timeout = 0;
  int failures = 0;
};

std::filesystem::path ProjectRoot() {
  return std::filesystem::path(__FILE__).parent_path().parent_path();
}

double Percentile(std::vector<double> values, double ratio) {
  if (values.empty()) {
    return 0.0;
  }
  std::sort(values.begin(), values.end());
  const auto index = static_cast<std::size_t>(ratio * static_cast<double>(values.size() - 1));
  return values[index];
}

ScenarioResult Summarize(const std::string& scenario,
                         int concurrency,
                         int payload_bytes,
                         int total_ops,
                         const std::chrono::steady_clock::duration& elapsed,
                         std::vector<ThreadStats> stats) {
  ScenarioResult result;
  result.scenario = scenario;
  result.concurrency = concurrency;
  result.payload_bytes = payload_bytes;
  result.total_ops = total_ops;
  result.elapsed_ms =
      std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(elapsed).count();

  std::vector<double> latencies;
  for (auto& item : stats) {
    result.success += item.success;
    result.busy += item.busy;
    result.timeout += item.timeout;
    result.failures += item.failures;
    latencies.insert(latencies.end(), item.latencies_us.begin(), item.latencies_us.end());
  }
  const auto total_success = std::max(result.success, 1);
  result.qps = result.success * 1000.0 / std::max(result.elapsed_ms, 1.0);
  result.avg_us = std::accumulate(latencies.begin(), latencies.end(), 0.0) /
                  static_cast<double>(total_success);
  result.p50_us = Percentile(latencies, 0.50);
  result.p95_us = Percentile(latencies, 0.95);
  result.p99_us = Percentile(latencies, 0.99);
  return result;
}

void WriteCsv(const std::vector<ScenarioResult>& results, const std::filesystem::path& output) {
  std::filesystem::create_directories(output.parent_path());
  std::ofstream stream(output);
  stream << "scenario,concurrency,payload_bytes,total_ops,success,busy,timeout,failures,"
            "elapsed_ms,qps,avg_us,p50_us,p95_us,p99_us\n";
  stream << std::fixed << std::setprecision(2);
  for (const auto& result : results) {
    stream << result.scenario << ','
           << result.concurrency << ','
           << result.payload_bytes << ','
           << result.total_ops << ','
           << result.success << ','
           << result.busy << ','
           << result.timeout << ','
           << result.failures << ','
           << result.elapsed_ms << ','
           << result.qps << ','
           << result.avg_us << ','
           << result.p50_us << ','
           << result.p95_us << ','
           << result.p99_us << '\n';
  }
}

void PrintSummary(const ScenarioResult& result) {
  std::cout << std::left << std::setw(22) << result.scenario
            << " qps=" << std::setw(10) << std::fixed << std::setprecision(2) << result.qps
            << " avg_us=" << std::setw(10) << result.avg_us
            << " p99_us=" << std::setw(10) << result.p99_us
            << " success=" << result.success
            << " busy=" << result.busy
            << " timeout=" << result.timeout
            << std::endl;
}

ScenarioResult RunLocalAddBaseline(int total_ops) {
  demo::DemoCalculatorService service;
  std::vector<double> latencies;
  latencies.reserve(total_ops);

  const auto start = Clock::now();
  for (int i = 0; i < total_ops; ++i) {
    demo::AddRequest request;
    request.lhs = i;
    request.rhs = i + 1;
    demo::AddResponse response;
    const auto op_start = Clock::now();
    const auto status = service.Add(request, &response);
    const auto op_end = Clock::now();
    if (!status.ok() || response.result != request.lhs + request.rhs) {
      throw std::runtime_error("local baseline validation failed");
    }
    latencies.push_back(
        std::chrono::duration_cast<std::chrono::duration<double, std::micro>>(op_end - op_start)
            .count());
  }
  const auto end = Clock::now();
  ThreadStats stats;
  stats.latencies_us = std::move(latencies);
  stats.success = total_ops;
  return Summarize("local_add_baseline", 1, 0, total_ops, end - start, {std::move(stats)});
}

ScenarioResult RunAddScenario(const std::string& scenario,
                              const mrpc::Endpoint& endpoint,
                              int concurrency,
                              int ops_per_thread) {
  std::vector<std::thread> threads;
  std::vector<ThreadStats> stats(static_cast<std::size_t>(concurrency));

  const auto start = Clock::now();
  for (int i = 0; i < concurrency; ++i) {
    threads.emplace_back([&, i]() {
      mrpc::RpcChannel channel(endpoint, "mini-secret");
      demo::CalculatorServiceClient client(&channel);
      auto& local = stats[static_cast<std::size_t>(i)];
      local.latencies_us.reserve(ops_per_thread);

      for (int op = 0; op < ops_per_thread; ++op) {
        demo::AddRequest request;
        request.lhs = i + op;
        request.rhs = op;
        demo::AddResponse response;
        const auto op_start = Clock::now();
        const auto status = client.Add(request, &response, 2000);
        const auto op_end = Clock::now();
        local.latencies_us.push_back(
            std::chrono::duration_cast<std::chrono::duration<double, std::micro>>(op_end -
                                                                                   op_start)
                .count());
        if (status.ok() && response.result == request.lhs + request.rhs) {
          ++local.success;
        } else if (status.code() == mrpc::StatusCode::kBusy) {
          ++local.busy;
        } else if (status.code() == mrpc::StatusCode::kTimeout) {
          ++local.timeout;
        } else {
          ++local.failures;
        }
      }
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  const auto end = Clock::now();
  return Summarize(
      scenario, concurrency, 8, concurrency * ops_per_thread, end - start, std::move(stats));
}

ScenarioResult RunEchoScenario(const std::string& scenario,
                               const mrpc::Endpoint& endpoint,
                               int concurrency,
                               int ops_per_thread,
                               int payload_bytes) {
  std::vector<std::thread> threads;
  std::vector<ThreadStats> stats(static_cast<std::size_t>(concurrency));
  const std::string payload(static_cast<std::size_t>(payload_bytes), 'x');

  const auto start = Clock::now();
  for (int i = 0; i < concurrency; ++i) {
    threads.emplace_back([&, i]() {
      mrpc::RpcChannel channel(endpoint, "mini-secret");
      demo::CalculatorServiceClient client(&channel);
      auto& local = stats[static_cast<std::size_t>(i)];
      local.latencies_us.reserve(ops_per_thread);

      for (int op = 0; op < ops_per_thread; ++op) {
        demo::EchoRequest request;
        request.payload = payload;
        demo::EchoResponse response;
        const auto op_start = Clock::now();
        const auto status = client.Echo(request, &response, 2000);
        const auto op_end = Clock::now();
        local.latencies_us.push_back(
            std::chrono::duration_cast<std::chrono::duration<double, std::micro>>(op_end -
                                                                                   op_start)
                .count());
        if (status.ok() && response.payload == payload) {
          ++local.success;
        } else if (status.code() == mrpc::StatusCode::kBusy) {
          ++local.busy;
        } else if (status.code() == mrpc::StatusCode::kTimeout) {
          ++local.timeout;
        } else {
          ++local.failures;
        }
      }
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  const auto end = Clock::now();
  return Summarize(
      scenario, concurrency, payload_bytes, concurrency * ops_per_thread, end - start,
      std::move(stats));
}

ScenarioResult RunOverloadScenario(const mrpc::Endpoint& endpoint,
                                   int concurrency,
                                   int ops_per_thread,
                                   int sleep_ms) {
  std::vector<std::thread> threads;
  std::vector<ThreadStats> stats(static_cast<std::size_t>(concurrency));

  const auto start = Clock::now();
  for (int i = 0; i < concurrency; ++i) {
    threads.emplace_back([&, i]() {
      mrpc::RpcChannel channel(endpoint, "mini-secret");
      demo::CalculatorServiceClient client(&channel);
      auto& local = stats[static_cast<std::size_t>(i)];
      local.latencies_us.reserve(ops_per_thread);

      for (int op = 0; op < ops_per_thread; ++op) {
        demo::DelayRequest request;
        request.sleep_ms = sleep_ms;
        demo::DelayResponse response;
        const auto op_start = Clock::now();
        const auto status = client.Delay(request, &response, 200);
        const auto op_end = Clock::now();
        local.latencies_us.push_back(
            std::chrono::duration_cast<std::chrono::duration<double, std::micro>>(op_end -
                                                                                   op_start)
                .count());
        if (status.ok()) {
          ++local.success;
        } else if (status.code() == mrpc::StatusCode::kBusy) {
          ++local.busy;
        } else if (status.code() == mrpc::StatusCode::kTimeout) {
          ++local.timeout;
        } else {
          ++local.failures;
        }
      }
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  const auto end = Clock::now();
  return Summarize(
      "rpc_delay_overload_c16", concurrency, 4, concurrency * ops_per_thread, end - start,
      std::move(stats));
}

}  // namespace

int main() {
  std::vector<ScenarioResult> results;

  mrpc::ServerOptions standard_options;
  standard_options.host = "127.0.0.1";
  standard_options.port = 0;
  standard_options.worker_threads = 4;
  standard_options.max_queue_size = 256;
  standard_options.auth_token = "mini-secret";

  mrpc::RpcServer standard_server(standard_options);
  demo::DemoCalculatorService service;
  auto status = demo::RegisterCalculatorService(&standard_server, &service);
  if (!status.ok()) {
    std::cerr << "register service failed: " << status.message() << '\n';
    return 1;
  }
  status = standard_server.Start();
  if (!status.ok()) {
    std::cerr << "start standard server failed: " << status.message() << '\n';
    return 1;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  const mrpc::Endpoint standard_endpoint{"127.0.0.1", standard_server.bound_port()};

  auto run_and_print = [&](const std::string& name, auto&& runner) {
    std::cout << "running " << name << " ..." << std::endl;
    ScenarioResult result = runner();
    PrintSummary(result);
    results.push_back(std::move(result));
  };

  run_and_print("local_add_baseline", [&]() { return RunLocalAddBaseline(50000); });
  run_and_print("rpc_add_c1", [&]() { return RunAddScenario("rpc_add_c1", standard_endpoint, 1, 500); });
  run_and_print("rpc_add_c2", [&]() { return RunAddScenario("rpc_add_c2", standard_endpoint, 2, 500); });
  run_and_print("rpc_add_c4", [&]() { return RunAddScenario("rpc_add_c4", standard_endpoint, 4, 500); });
  run_and_print("rpc_add_c8", [&]() { return RunAddScenario("rpc_add_c8", standard_endpoint, 8, 400); });
  run_and_print("rpc_echo_64b_c4", [&]() { return RunEchoScenario("rpc_echo_64b_c4", standard_endpoint, 4, 300, 64); });
  run_and_print("rpc_echo_1024b_c4", [&]() { return RunEchoScenario("rpc_echo_1024b_c4", standard_endpoint, 4, 200, 1024); });
  run_and_print("rpc_echo_4096b_c4", [&]() { return RunEchoScenario("rpc_echo_4096b_c4", standard_endpoint, 4, 100, 4096); });
  standard_server.Stop();

  mrpc::ServerOptions overload_options;
  overload_options.host = "127.0.0.1";
  overload_options.port = 0;
  overload_options.worker_threads = 2;
  overload_options.max_queue_size = 8;
  overload_options.auth_token = "mini-secret";

  mrpc::RpcServer overload_server(overload_options);
  status = demo::RegisterCalculatorService(&overload_server, &service);
  if (!status.ok()) {
    std::cerr << "register overload service failed: " << status.message() << '\n';
    return 1;
  }
  status = overload_server.Start();
  if (!status.ok()) {
    std::cerr << "start overload server failed: " << status.message() << '\n';
    return 1;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  const mrpc::Endpoint overload_endpoint{"127.0.0.1", overload_server.bound_port()};
  run_and_print("rpc_delay_overload_c16", [&]() {
    return RunOverloadScenario(overload_endpoint, 16, 10, 20);
  });
  overload_server.Stop();

  const auto output = ProjectRoot() / "benchmarks/results/latest.csv";
  WriteCsv(results, output);

  std::cout << "benchmark csv written to " << output << '\n';
  return 0;
}
