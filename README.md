# MiniRPC

一个适合简历展示的轻量级 C++ RPC 项目：从零实现自定义二进制协议、IDL 驱动的客户端/服务端代码生成、基于 TCP 的同步调用链路、静态服务发现、鉴权、超时控制、快速拒绝、测试与基准报告。

## 项目亮点

- 自定义传输协议：固定头 + 可变长包体，携带 `request_id`、消息类型、负载长度等元数据。
- IDL + 代码生成：`tools/mrpcgen.py` 解析 `idl/*.mproto`，自动生成消息结构体、客户端 Stub、服务端注册胶水代码。
- TCP 通信层：连接复用、请求超时、`TCP_NODELAY`、header/body 合并发送。
- 服务治理最小闭环：配置式注册发现、鉴权 token、IP 白名单、线程池执行器、队列满时快速返回 `BUSY`。
- 可验证：包含单元测试、集成测试、性能基准、性能报告和架构/面试文档。

## 目录结构

- `include/minirpc/`：运行时核心组件
- `src/`：协议、通道、服务端、线程池、配置与注册发现实现
- `idl/`：IDL 定义
- `tools/`：代码生成器与性能报告生成器
- `examples/`：示例服务实现、服务端和客户端
- `tests/`：运行时与端到端测试
- `benchmarks/`：性能压测代码与输出数据
- `docs/`：架构、API、技术栈、难点、面试题、性能报告、横向对比

## 快速开始

```bash
cmake -S . -B build
cmake --build build -j 2
ctest --test-dir build --output-on-failure
```

启动服务端：

```bash
./build/calculator_server configs/server.ini
```

启动客户端：

```bash
./build/calculator_client configs/registry.ini
```

运行基准：

```bash
./build/benchmark_rpc
```

## 当前性能快照

- `rpc_add_c8`: `19892.11 QPS`, `avg=369.92 us`, `p99=951.86 us`
- `rpc_echo_4096b_c4`: `14094.19 QPS`, `avg=268.32 us`, `p99=768.72 us`
- `rpc_delay_overload_c16`: `100/160` 成功，`60/160` 被快速拒绝，证明过载保护生效

详细数据见 `docs/perf-report.md`。

## 支持的 IDL 子集

`MiniRPC` 目前支持 protobuf 风格的精简 IDL 子集：

```proto
package demo;

message AddRequest {
  int32 lhs = 1;
  int32 rhs = 2;
}

service CalculatorService {
  rpc Add(AddRequest) returns (AddResponse);
}
```

支持字段类型：`bool`、`int32`、`int64`、`uint32`、`uint64`、`double`、`string`。

## 文档索引

- 架构说明：`docs/architecture.md`
- API 与协议说明：`docs/api.md`
- 技术栈：`docs/tech-stack.md`
- 学习与面试深度解析：`docs/learning/minirpc-deep-dive.md`
- 实现重难点：`docs/challenges.md`
- 性能报告：`docs/perf-report.md`
- 常见面试题：`docs/interview-questions.md`
- 与主流 RPC 框架横向对比：`docs/comparison.md`
