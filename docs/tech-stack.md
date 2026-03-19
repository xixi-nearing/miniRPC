# 技术栈清单

## 核心语言与工程能力

- `C++20`：框架核心实现
- `CMake 3.28+`：构建系统
- `POSIX Socket`：TCP 通信
- `Python 3`：IDL 代码生成器与基准报告生成器

## 框架核心组件

- 自定义二进制协议：固定头 + 可变长包体
- protobuf 风格精简 IDL：`.mproto`
- 自研 Stub 生成器：`tools/mrpcgen.py`
- 运行时组件：
  - `RpcServer`
  - `RpcChannel`
  - `StaticRegistry`
  - `ThreadPool`
  - `BufferWriter / BufferReader`
  - `RpcStatus`

## 工程能力

- 单元测试：协议、配置、注册发现
- 集成测试：服务启动、鉴权、超时、重复调用
- 性能测试：QPS、平均延迟、P50/P95/P99、过载保护
- 文档化输出：架构说明、API、技术栈、难点、面试题、横向对比

## 可以写进简历的关键词

- RPC Framework
- Custom Binary Protocol
- IDL / Code Generation
- TCP Networking
- Multithreaded Server
- Service Discovery
- Timeout / Auth / Fast Reject
- Performance Benchmarking
