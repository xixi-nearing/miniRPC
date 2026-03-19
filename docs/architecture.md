# MiniRPC 架构说明

## 1. 目标

MiniRPC 的目标不是替代生产级 RPC 中间件，而是把一个 RPC 框架的核心链路完整走通，并且代码量适中、便于讲解和扩展，适合作为简历项目与面试项目。

## 2. 核心模块

### 2.1 IDL 与代码生成

- 输入：`idl/calculator.mproto`
- 生成器：`tools/mrpcgen.py`
- 输出：
  - 消息结构体
  - 消息序列化 / 反序列化代码
  - 客户端 Stub
  - 服务端注册胶水代码

这部分模拟了 Protobuf + `protoc` 在生产框架中的角色，但这里为了可控性采用自研的 protobuf 风格精简语法。

### 2.2 协议层

每个 RPC 调用都被编码为：

1. 固定长度帧头 `FrameHeader`
2. 可变长请求体 / 响应体

帧头负责快速判定消息边界，包体负责承载服务名、方法名、鉴权 token、deadline 和业务 payload。

### 2.3 序列化层

- 框架协议使用大端序编码
- 生成代码按字段顺序进行二进制编解码
- `string` 类型采用 `uint32 length + bytes` 形式

这样实现复杂度低、性能稳定，也便于解释“协议设计”和“序列化格式设计”。

### 2.4 通信层

- 传输协议：TCP
- 客户端：`mrpc::RpcChannel`
- 服务端：`mrpc::RpcServer`
- 关键点：
  - 连接复用
  - 每次调用设置 socket 超时
  - `TCP_NODELAY`
  - header/body 合并发送，降低小包延迟

### 2.5 服务发现

当前版本采用静态注册发现：

- `configs/server.ini`：服务端监听与鉴权配置
- `configs/registry.ini`：客户端查询服务地址
- `mrpc::StaticRegistry`：根据服务名解析 `host:port`

这部分提供了服务发现抽象，未来可替换为 etcd / ZooKeeper / Consul。

### 2.6 并发处理

服务端采用：

- 一个 accept 线程
- 每个连接一个读写线程
- 一个固定大小线程池执行业务逻辑

这样的设计可以把“连接管理”和“业务执行”解耦，线程池队列满时还能直接返回 `BUSY`。

### 2.7 错误处理与安全

- `RpcStatus` + `StatusCode`
- 支持 `UNKNOWN_SERVICE` / `UNKNOWN_METHOD` / `TIMEOUT` / `AUTH_FAILED` / `BUSY` 等错误码
- 基于 token 的简单鉴权
- 支持 IP allowlist

## 3. 调用链路

### 3.1 客户端调用

1. 业务代码调用生成的客户端 Stub
2. Stub 把请求结构体序列化为二进制
3. `RpcChannel` 组装请求帧并发送
4. 等待响应帧并解析
5. Stub 反序列化响应并返回

### 3.2 服务端处理

1. `RpcServer` 接收 TCP 连接
2. 读取并解析帧头/帧体
3. 校验鉴权与 deadline
4. 按 `service + method` 查找处理函数
5. 在线程池中执行服务实现
6. 序列化响应并回写客户端

## 4. 为什么这样设计

- 不直接依赖外部重量级 RPC 组件，便于完全掌控代码与原理
- 保留 IDL、Stub、服务注册、序列化、网络层、并发、错误处理这些核心面试点
- 静态注册发现与自定义二进制协议，使项目足够完整但复杂度可控

## 5. 可扩展方向

- 用 epoll 替换线程级连接处理
- 支持异步 future / callback API
- 支持压缩、重试、熔断、限流、链路追踪
- 抽象注册中心接口并接入 etcd / ZooKeeper
- 扩展 IDL 语法，支持 `bytes`、`repeated`、嵌套消息
