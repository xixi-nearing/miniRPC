# MiniRPC 与主流 RPC 框架横向对比

> 参考官方资料：gRPC Docs、bRPC Docs、Apache Thrift Tutorial、Apache Dubbo 官网。

## 1. 对比结论

MiniRPC 不追求“生产能力全覆盖”，而是追求：

- 从零打通 RPC 核心链路
- 代码量可控，适合深度讲解
- 可直接展示协议、Stub、并发、性能优化与工程验证能力

所以它的核心优势不是生态，而是“可解释性 + 可扩展性 + 项目完整度”。

## 2. 对比表

| 维度 | MiniRPC | gRPC | bRPC | Apache Thrift | Apache Dubbo |
| --- | --- | --- | --- | --- | --- |
| 语言定位 | C++ 自研教学/简历项目 | 多语言工业级 RPC | C++ 高性能 RPC | 多语言 RPC/序列化框架 | 微服务治理框架 |
| IDL | protobuf 风格精简子集 | Protobuf | 常见配合 Protobuf，且支持多协议 | Thrift IDL | 多语言 IDL，常见配合 Protobuf/JSON |
| 代码生成 | 自研生成器 | 官方 `protoc` 生态成熟 | 工具链成熟 | 官方编译器成熟 | 生态成熟 |
| 传输协议 | 自定义二进制 + TCP | HTTP/2 | 多协议、多 naming service | 多协议 | 多协议 + 服务治理 |
| 服务发现 | 静态配置 | DNS / xDS / 自定义 name resolver | naming service 丰富 | 需自行组合 | 注册中心生态完善 |
| 并发模型 | 连接线程 + 业务线程池 | 成熟异步/流控机制 | 高性能工业级实现 | 框架级支持 | 面向微服务治理 |
| 过载保护 | 有界队列 + `BUSY` | 依赖完整治理栈 | 完整能力更强 | 依赖具体实现 | 治理能力最完整 |
| 可读性 | 很高 | 中 | 中 | 中 | 中偏低 |
| 学习成本 | 低 | 中 | 中高 | 中 | 高 |
| 生产适用性 | 低 | 高 | 高 | 高 | 高 |

## 3. MiniRPC 的核心优势

### 优势 1：从零可控

- 协议、序列化、Stub、服务端分发、超时、鉴权都是自己实现
- 面试时可以讲清楚每一层为什么这样设计

### 优势 2：轻量但完整

- 不是“只写一个 socket demo”
- 具备 IDL、代码生成、服务发现、并发、测试、压测、文档这些完整项目要素

### 优势 3：优化故事真实

- 通过基准测试发现小包延迟问题
- 用 `TCP_NODELAY` + 单次发送优化解决
- 这类“测出来 -> 定位 -> 优化 -> 回归验证”的闭环非常适合简历和面试

### 优势 4：扩展空间明确

- 很容易继续往 epoll、异步调用、注册中心、链路追踪方向演进
- 能自然讲出下一阶段 roadmap

## 4. 什么时候用 MiniRPC，什么时候不用

适合：

- 简历项目
- RPC 原理学习
- 面试展示
- 教学演示

不适合：

- 直接替代生产级 gRPC / bRPC / Dubbo
- 复杂跨语言微服务体系
- 大规模治理、流量调度、服务网格集成场景

## 5. 官方资料

- gRPC: https://grpc.io/docs/
- bRPC: https://brpc.apache.org/docs/
- Apache Thrift: https://thrift.apache.org/tutorial
- Apache Dubbo: https://dubbo.apache.org/en/overview/what/
