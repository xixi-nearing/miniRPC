# MiniRPC 学习与面试深度解析

这份文档面向两个场景：

- 学习 RPC 框架时，快速建立“项目做了什么、为什么这样做、具体怎么做”的完整认知。
- 面试介绍项目时，把技术栈、设计思路、实现细节、优化点和边界讲清楚。

它不是对 `docs/architecture.md`、`docs/api.md`、`docs/tech-stack.md` 的简单重复，而是把这些内容重新组织成“更适合学习和表达”的版本。

---

## 1. 项目定位

`MiniRPC` 是一个轻量级 C++ RPC 项目，目标不是替代 gRPC 或 bRPC，而是从零实现 RPC 的核心闭环，并把代码量控制在适合阅读、讲解和扩展的范围内。

这个项目已经打通的链路包括：

- IDL 定义接口契约
- Python 生成客户端 Stub 和服务端胶水代码
- 自定义二进制协议完成请求编解码
- TCP 连接承载同步 RPC 调用
- 静态注册发现提供服务定位
- 服务端线程池提供并发执行和过载保护
- 鉴权、超时、IP 白名单、错误码完成最小治理闭环
- 单测、集成测试、基准测试、性能报告支撑验证

如果用一句话概括它的价值，可以这样说：

> 它不是“会调用现成 RPC 框架”，而是“自己实现过 RPC 框架的关键机制”。

---

## 2. 技术栈总览

### 2.1 语言与构建

| 技术 | 用途 | 在项目中的位置 |
| --- | --- | --- |
| `C++20` | 运行时框架实现 | `include/minirpc/`、`src/` |
| `CMake` | 构建、代码生成接入、测试编排 | `CMakeLists.txt`、`cmake/MiniRpcCodegen.cmake` |
| `Python 3` | IDL 解析和代码生成、性能报告生成 | `tools/mrpcgen.py`、`tools/render_benchmark_report.py` |

### 2.2 运行时能力

| 技术 | 用途 | 对应模块 |
| --- | --- | --- |
| `POSIX Socket` | TCP 建连、收发、监听 | `src/channel.cpp`、`src/server.cpp` |
| 自定义二进制协议 | 解决消息边界、元数据承载 | `include/minirpc/protocol.h`、`src/protocol.cpp` |
| 自研序列化 | 消息对象和二进制之间转换 | `include/minirpc/buffer.h`、生成代码 |
| 线程池 | 并发执行和限流入口 | `include/minirpc/thread_pool.h`、`src/thread_pool.cpp` |
| 静态注册发现 | 客户端按服务名找到地址 | `include/minirpc/registry.h`、`src/registry.cpp` |

### 2.3 工程化能力

| 技术 | 用途 | 对应位置 |
| --- | --- | --- |
| 单元测试 | 验证协议、配置、注册发现 | `tests/test_runtime.cpp` |
| 集成测试 | 验证真实 RPC 链路 | `tests/test_rpc_integration.cpp` |
| 基准测试 | 测量 QPS、延迟、过载行为 | `benchmarks/benchmark_rpc.cpp` |
| 文档 | 支撑学习、汇报、面试表达 | `docs/` |

---

## 3. 模块职责拆解

### 3.1 协议层

- `include/minirpc/protocol.h`
- `src/protocol.cpp`

职责：

- 定义固定长度协议头 `FrameHeader`
- 定义请求体 `RequestFrame` 和响应体 `ResponseFrame`
- 提供 header/body 编解码函数
- 提供 `NowUnixMillis()` 作为 deadline 基础

核心意义：

- 让 TCP 字节流具备明确消息边界
- 把请求 ID、消息类型、长度、状态码等元信息显式化

### 3.2 序列化层

- `include/minirpc/buffer.h`

职责：

- `BufferWriter` 负责按大端序把整数、浮点、字符串写入缓冲区
- `BufferReader` 负责按相同规则读取数据

核心意义：

- 给协议层和生成代码提供统一底层编解码工具
- 降低手写字节偏移和类型转换错误

### 3.3 客户端通信层

- `include/minirpc/channel.h`
- `src/channel.cpp`

职责：

- 封装一个到单个 `Endpoint` 的同步调用通道
- 负责建连、连接复用、超时设置、请求发送、响应接收、异常重置

核心意义：

- 把“像本地方法调用一样使用 RPC”的体验统一收敛到 `RpcChannel::Call`

### 3.4 服务端执行层

- `include/minirpc/server.h`
- `src/server.cpp`

职责：

- 监听 TCP 端口并接收连接
- 读取请求帧并做基本校验
- 按 `service + method` 分发到对应处理函数
- 用线程池异步执行业务逻辑
- 回写响应帧

核心意义：

- 把网络读写、服务路由、业务执行分成不同职责

### 3.5 并发执行层

- `include/minirpc/thread_pool.h`
- `src/thread_pool.cpp`

职责：

- 维护固定大小工作线程
- 维护有界任务队列
- 队列满时拒绝新请求

核心意义：

- 实现“有节制的并发”，而不是无限排队

### 3.6 配置与注册发现

- `include/minirpc/config.h`
- `src/config.cpp`
- `include/minirpc/registry.h`
- `src/registry.cpp`
- `configs/server.ini`
- `configs/registry.ini`

职责：

- 加载 INI 配置
- 解析服务监听参数、鉴权 token、白名单、客户端超时
- 根据服务名查到 `host:port`

核心意义：

- 给框架留出服务发现抽象，而不是把地址硬编码到客户端

### 3.7 IDL 与代码生成

- `idl/calculator.mproto`
- `tools/mrpcgen.py`
- `cmake/MiniRpcCodegen.cmake`
- `build/generated/calculator_rpc.h`
- `build/generated/calculator_rpc.cpp`

职责：

- 解析精简版 protobuf 风格 IDL
- 生成消息结构体
- 生成消息序列化/反序列化函数
- 生成服务抽象类
- 生成客户端 Stub 和服务端注册胶水代码

核心意义：

- 把“接口契约”从业务逻辑和网络细节中分离出来

---

## 4. 项目用了哪些设计思想

### 4.1 契约驱动开发

定义：

- 先用 IDL 描述接口，再自动生成客户端和服务端框架代码。

实现方式：

- `idl/calculator.mproto` 定义消息和服务。
- `tools/mrpcgen.py` 解析 token，产出 `*_rpc.h` 和 `*_rpc.cpp`。
- 业务侧只需要继承生成的 `CalculatorService` 并实现业务方法。

优势：

- 客户端和服务端共享一份接口契约
- 减少手写样板代码
- 面试中可以自然引出“为什么 RPC 需要 IDL”

### 4.2 分层解耦

定义：

- 协议、网络、序列化、服务路由、业务实现彼此隔离。

实现方式：

- `protocol` 只关心帧结构
- `channel` 只关心调用链路
- `server` 只关心接收、分发和回包
- `thread_pool` 只关心任务执行
- 生成代码连接“业务对象”和“RPC 运行时”

优势：

- 每层职责清楚
- 调试时容易定位问题
- 后续扩展更自然，比如把静态注册发现替换成 etcd，把线程模型替换成 epoll

### 4.3 固定头 + 可变长包体

定义：

- 用固定长度 header 解决 TCP 没有消息边界的问题，再根据 `body_length` 读取完整 body。

实现方式：

- `FrameHeader` 固定为 20 字节
- `DecodeHeader` 先验证 magic/version
- `ReadFully` 先读 header，再读 body

优势：

- 简单直接，便于讲解
- 易于扩展字段
- 服务端可以快速识别非法请求

### 4.4 连接复用

定义：

- 同一个客户端通道不为每次 RPC 都重新建 TCP 连接，而是复用已有连接。

实现方式：

- `RpcChannel::EnsureConnected()` 仅在 `socket_fd_ < 0` 时建连
- 多次 `Call` 复用同一个 socket
- 发生超时、读写失败或响应异常时 `Reset()`

优势：

- 减少频繁三次握手和资源开销
- 更贴近真实 RPC 框架的连接管理思路

### 4.5 Fail-Fast 过载保护

定义：

- 线程池有界，队列满时立刻返回错误，不让请求无上限堆积。

实现方式：

- `ThreadPool::Submit()` 在 `queue_.size() >= max_queue_size_` 时直接返回 `false`
- 服务端收到 `false` 后响应 `StatusCode::kBusy`

优势：

- 比无限排队更符合服务治理思想
- 在压测时能直接看到“保护系统而不是拖死系统”的行为

### 4.6 显式错误建模

定义：

- 用统一状态对象而不是异常跨层传播错误。

实现方式：

- `StatusCode` 定义业务和框架错误码
- `RpcStatus` 封装 `code + message`
- 框架和业务都通过 `RpcStatus` 返回执行结果

优势：

- 错误路径可读性高
- 便于跨网络传递状态
- 面试里可以说明为什么 RPC 系统常用错误码而不是直接抛异常

---

## 5. 一次 RPC 调用的完整实现流程

### 5.1 客户端视角

1. 业务代码创建 `CalculatorServiceClient`
2. 调用 `client.Add(request, &response, timeout_ms)`
3. 生成 Stub 先调用 `request.SerializeToString()`
4. Stub 再调用 `RpcChannel::Call(...)`
5. `RpcChannel` 组装 `RequestFrame`
6. `EncodeRequestBody()` 编码包体
7. `EncodeHeader()` 编码包头
8. header 和 body 拼成一次发送缓冲区
9. 客户端读取响应 header 和 body
10. `DecodeResponseBody()` 解析状态和 payload
11. Stub 调用 `response->DeserializeFromString(...)`
12. 业务代码获得强类型响应对象

### 5.2 服务端视角

1. `RpcServer::Start()` 创建监听 socket 并启动 accept 线程
2. `AcceptLoop()` 接收连接并为每个连接起一个处理线程
3. `HandleClient()` 循环读取 header/body
4. `DecodeRequestBody()` 还原 `service`、`method`、`auth_token`、`deadline`、`payload`
5. 服务端把任务提交给线程池
6. 线程池中的任务调用 `Dispatch()`
7. `Dispatch()` 依次做鉴权、deadline 校验、服务查找、方法查找
8. 命中方法后执行生成的注册胶水函数
9. 胶水函数把 payload 反序列化为强类型 request，调用真实业务实现
10. 业务返回后把 response 再序列化为 payload
11. 服务端回写响应帧

### 5.3 这个流程体现了什么

- 业务层没有直接处理 socket
- 客户端像调用本地函数一样发 RPC
- 服务端路由完全依赖服务名和方法名
- 网络协议与业务消息结构是解耦的

---

## 6. 项目已经实现了哪些功能

### 6.1 自定义二进制协议

功能定义：

- 框架自己定义消息格式，而不是依赖 HTTP/2 或第三方序列化协议。

实现细节：

- 协议头包括魔数、版本、消息类型、编解码类型、标记位、请求 ID、包体长度
- 请求体包括服务名、方法名、鉴权 token、截止时间、业务 payload
- 响应体包括状态码、消息、业务 payload

### 6.2 自动代码生成

功能定义：

- 输入 IDL，自动生成消息类型、客户端、服务端胶水层。

实现细节：

- `mrpcgen.py` 使用正则切 token，再通过简单递归下降风格解析
- `TYPE_MAP` 维护 IDL 类型到 C++ 类型和读写方法的映射
- 生成器既生成头文件，也生成源文件
- CMake 用 `add_custom_command` 把生成流程接入构建

### 6.3 同步 RPC 调用

功能定义：

- 客户端在当前线程发请求、等待响应、得到结果。

实现细节：

- `RpcChannel::Call()` 串行完成发送和接收
- 使用互斥锁保护同一个 `RpcChannel` 的并发访问
- 响应的 `request_id` 必须和请求一致

### 6.4 静态服务发现

功能定义：

- 客户端按服务名查询服务地址，而不是把地址写死在代码里。

实现细节：

- `registry.ini` 中每个服务一个 section
- `StaticRegistry::Find()` 返回 `std::optional<Endpoint>`
- 如果 host/port 非法则返回空

### 6.5 鉴权

功能定义：

- 用共享 token 阻止未授权调用。

实现细节：

- 客户端在每个请求体中附带 `auth_token`
- 服务端 `Dispatch()` 比较 `request.auth_token` 和 `options_.auth_token`
- 不匹配则返回 `kAuthFailed`

### 6.6 超时控制

功能定义：

- 避免请求无限阻塞。

实现细节：

- 客户端每次调用前设置 `SO_RCVTIMEO` 和 `SO_SNDTIMEO`
- 请求体里写入 `deadline_unix_ms`
- 服务端在真正执行业务前再次校验 deadline

这说明项目里做了两层控制：

- 网络 I/O 超时
- 业务执行前 deadline 校验

### 6.7 IP 白名单

功能定义：

- 限制只有允许的来源 IP 能建立连接。

实现细节：

- 服务端配置读取 `allowlist`
- `AcceptLoop()` 在 accept 后解析对端 IP
- `IsAllowedPeer()` 判定是否允许，不通过则直接关闭连接

### 6.8 过载快速拒绝

功能定义：

- 服务端繁忙时尽快返回 `BUSY`，而不是无界堆积。

实现细节：

- 线程池队列大小由 `max_queue_size` 控制
- `Submit()` 失败后，服务端直接返回 `"executor queue is full"`

### 6.9 示例业务能力

项目示例服务 `CalculatorService` 包含：

- `Add`: 两数相加
- `Fib`: 计算斐波那契数列
- `Echo`: 原样回显字符串
- `Delay`: 睡眠指定时间，便于验证超时和过载场景

这些接口分别覆盖了：

- 基础数值请求
- CPU 计算型请求
- 字符串与变长负载
- 长耗时请求

---

## 7. 关键库函数、系统调用和注意事项

这一部分是面试里非常容易被追问的点。

### 7.1 `socket`

用途：

- 创建 TCP socket。

项目里怎么用：

- 客户端和服务端都通过 `::socket(AF_INET, SOCK_STREAM, 0)` 创建描述符。

注意事项：

- 创建失败要及时返回错误
- 后续任何一步失败都要关闭 fd，避免资源泄漏

### 7.2 `connect`

用途：

- 客户端发起 TCP 连接。

项目里怎么用：

- `RpcChannel::EnsureConnected()` 建立到远端 `Endpoint` 的连接。

注意事项：

- 连接失败后要把 `socket_fd_` 置回无效状态
- 地址必须先通过 `inet_pton` 转换

### 7.3 `bind` / `listen` / `accept`

用途：

- 服务端监听端口并接收新连接。

项目里怎么用：

- `RpcServer::Start()` 里做 `bind` 和 `listen`
- `AcceptLoop()` 里循环 `accept`

注意事项：

- 服务端要设置 `SO_REUSEADDR`，避免端口短时间无法复用
- `accept` 失败要区分可忽略错误和退出条件

### 7.4 `send` / `recv`

用途：

- 发送和接收字节流。

项目里怎么用：

- `WriteFully()` 循环调用 `send`
- `ReadFully()` 循环调用 `recv`

注意事项：

- TCP 不是消息协议，不能假设一次 `send` 对应一次 `recv`
- 必须循环直到写满或读满
- 遇到 `EINTR` 要继续
- 返回 0 通常表示对端关闭连接

### 7.5 `setsockopt`

用途：

- 设置 socket 行为。

项目里怎么用：

- `SO_RCVTIMEO`、`SO_SNDTIMEO` 控制超时
- `TCP_NODELAY` 关闭 Nagle
- `SO_REUSEADDR` 支持快速重启服务

注意事项：

- `TCP_NODELAY` 常用于降低小包延迟，但会增加报文数量
- 超时是对 I/O 行为生效，不等于一定能中断服务端业务执行

### 7.6 `shutdown` / `close`

用途：

- 关闭 socket。

项目里怎么用：

- `RpcChannel::ResetUnlocked()`
- `RpcServer::Stop()`
- `HandleClient()` 退出时清理连接

注意事项：

- 只 `close` 不一定能表达“半关闭”意图，但对当前简单项目足够
- 重复关闭要小心 fd 状态

### 7.7 `inet_pton` / `inet_ntop` / `htons` / `ntohs`

用途：

- 地址转换和字节序处理。

项目里怎么用：

- `inet_pton` 把文本 IP 转为二进制地址
- `inet_ntop` 把对端地址转为字符串用于白名单校验
- `htons` / `ntohs` 处理端口字节序

注意事项：

- 网络相关整数统一考虑网络字节序
- `inet_pton` 返回值需要判错

### 7.8 `std::bit_cast`

用途：

- 在 `double` 与 `uint64_t` 位模式之间无损转换。

项目里怎么用：

- `BufferWriter::WriteDouble`
- `BufferReader::ReadDouble`

注意事项：

- 依赖 C++20
- 适合做二进制稳定编码，不要用未定义行为的强转替代

### 7.9 `std::make_unsigned_t` 和模板约束

用途：

- 把有符号整数统一转成无符号位表示进行编码。

项目里怎么用：

- `buffer.h` 里的 `UnsignedEquivalent`
- `WriteIntegral` / `ReadIntegral`

注意事项：

- `bool` 是特殊情况，项目单独映射成 `uint8_t`
- 二进制协议里要先约定类型宽度，再谈编码

### 7.10 `std::optional`

用途：

- 表达“可能找到，也可能找不到”的查找结果。

项目里怎么用：

- `StaticRegistry::Find()` 返回 `std::optional<Endpoint>`

注意事项：

- 这种接口语义比返回哨兵值更清晰
- 调用方必须显式判断 `has_value()`

### 7.11 `std::promise` / `std::future`

用途：

- 在线程池任务和连接线程之间传递响应结果。

项目里怎么用：

- `HandleClient()` 提交任务后等待 `future.get()`

注意事项：

- 这里本质上仍然是同步等待，只是把业务执行放到了线程池
- 如果追求更高并发，可进一步演进为事件驱动或真正异步响应

### 7.12 `std::condition_variable`

用途：

- 让线程池工作线程在队列为空时阻塞等待。

项目里怎么用：

- `ThreadPool::WorkerLoop()` 中 `cv_.wait(...)`

注意事项：

- 一定要配合条件谓词避免虚假唤醒
- 停止线程池时要 `notify_all()`

---

## 8. 这个项目有哪些实现细节值得重点讲

### 8.1 为什么 header 和 body 合并发送

如果 header 和 body 分两次发送，小包场景下容易遇到 Nagle 和 delayed ACK 带来的额外延迟。

项目里的处理：

- 客户端和服务端都开启 `TCP_NODELAY`
- 发送前先把 header 和 body 拼成一块连续缓冲区，再一次 `send`

这类优化非常适合面试表达，因为它具备完整故事链：

- 先压测发现问题
- 再定位网络行为
- 然后做针对性优化
- 最后回归验证

### 8.2 为什么客户端超时后要重置连接

如果请求超时后仍继续复用同一连接，可能会出现响应错位：

- 当前调用超时了，但服务端之后才返回结果
- 下一个调用读取到的是上一个调用的残留响应

项目里的处理：

- 一旦读写失败、超时或响应帧异常，客户端立即 `Reset()`

这是一个典型的“保护连接一致性”的设计点。

### 8.3 为什么服务端不用异常做主流程控制

项目统一使用 `RpcStatus`，原因是：

- 跨网络边界传递错误时，错误码比异常更稳定
- 对协议层、网络层、业务层来说，错误模型统一更容易维护

### 8.4 为什么线程池队列要有上限

无上限队列的后果通常是：

- 短期看起来“没有丢请求”
- 长期却会导致整体延迟飙升、内存上涨、调用方雪崩

项目选择的是更工程化的策略：

- 宁可明确返回 `BUSY`
- 也不接受失控排队

---

## 9. 测试、基准和验证说明

### 9.1 单元测试验证了什么

`tests/test_runtime.cpp` 主要验证：

- header 编解码是否正确
- request/response body 编解码是否正确
- 配置文件解析是否正确
- 静态注册发现是否正确

这类测试证明：

- 框架基础组件是可独立验证的

### 9.2 集成测试验证了什么

`tests/test_rpc_integration.cpp` 主要验证：

- 服务端启动和客户端调用是否打通
- 正确 token 是否能成功调用
- 错误 token 是否返回 `kAuthFailed`
- `Delay` 请求是否能触发超时
- 同一客户端是否能重复调用

这类测试证明：

- 项目不是单点组件可运行，而是端到端链路可运行

### 9.3 基准测试验证了什么

`benchmarks/benchmark_rpc.cpp` 和 `docs/perf-report.md` 主要验证：

- 不同并发度下的 `Add` 吞吐和延迟
- 不同负载大小下的 `Echo` 行为
- `Delay` 场景下线程池过载保护效果

面试时可以直接用这些数据回答：

- 做了性能验证，而不是只停留在功能跑通
- 知道吞吐、平均延迟、P99 的意义
- 知道有界队列对系统稳定性的价值

---

## 10. 项目边界与当前限制

这部分非常重要，因为它能体现你对项目成熟度的判断是清醒的。

当前没有实现的能力包括：

- `epoll` / reactor 模型
- 异步 RPC / callback / future API
- 流式 RPC
- 重试、熔断、限流、链路追踪
- 动态注册中心
- 标准 Protobuf 编译器生态
- 更复杂的 IDL 能力，比如 `repeated`、嵌套消息、`map`

为什么没做：

- 这个项目的目标是“把核心原理闭环做出来”
- 不是把所有工业能力一次堆满

如何表达更合理：

- 已经打通协议、Stub、服务发现、并发、治理、测试、基准这些核心能力
- 未来扩展方向明确，演进路径清晰

---

## 11. 面试时可以怎么介绍这个项目

### 11.1 一分钟版本

我做了一个轻量级 C++ RPC 框架，从零实现了自定义二进制协议、IDL 驱动的代码生成、TCP 同步调用、静态服务发现、线程池并发执行，以及鉴权、超时和过载保护。项目里不仅把调用链路跑通了，还做了单测、集成测试和 benchmark，并针对小包延迟做过 `TCP_NODELAY + 合并发送` 的优化。

### 11.2 三分钟版本

可以按这个顺序讲：

1. 为什么做
2. 核心模块有哪些
3. 一次 RPC 调用链路怎么走
4. 解决了哪些工程问题
5. 做了哪些验证
6. 还有哪些明确的扩展方向

### 11.3 高频追问点

#### 为什么 RPC 需要 IDL

- 因为需要统一接口契约、自动生成样板代码、隔离业务和网络细节。

#### 为什么协议头要固定长度

- 因为 TCP 没有消息边界，固定头能先把长度和元数据读出来，再决定怎么读 body。

#### 为什么客户端超时后要断开连接

- 因为否则可能发生响应错位，污染后续请求。

#### 为什么要有界队列

- 因为比无限排队更能保护系统稳定性，也更符合服务治理思路。

#### 为什么不用 gRPC

- 因为这个项目的重点是展示底层原理理解和实现能力，而不是展示对成熟框架的调用经验。

### 11.4 最适合强调的亮点

- 从零实现 RPC 核心机制，而不是只写业务 demo
- 有 IDL 和代码生成，不是手写 socket 调接口
- 有鉴权、超时、快速拒绝这些框架能力
- 有测试和压测，能形成工程闭环
- 有明确性能优化故事，适合面试展开

---

## 12. 阅读源码的推荐顺序

如果你准备自己顺一遍项目源码，建议按下面顺序看：

1. `README.md`
2. `idl/calculator.mproto`
3. `tools/mrpcgen.py`
4. `build/generated/calculator_rpc.h`
5. `include/minirpc/buffer.h`
6. `include/minirpc/protocol.h`
7. `src/protocol.cpp`
8. `src/channel.cpp`
9. `src/server.cpp`
10. `src/thread_pool.cpp`
11. `examples/calculator_service_impl.cpp`
12. `tests/test_runtime.cpp`
13. `tests/test_rpc_integration.cpp`
14. `benchmarks/benchmark_rpc.cpp`

这样读的好处是：

- 先理解接口契约
- 再理解生成代码如何桥接运行时
- 最后看通信、调度、验证和性能

---

## 13. 这份项目最适合写进简历的关键词

- `C++20`
- `RPC Framework`
- `Custom Binary Protocol`
- `IDL / Code Generation`
- `TCP Networking`
- `Service Discovery`
- `Thread Pool`
- `Timeout / Auth / Fast Reject`
- `Unit Test / Integration Test / Benchmark`
- `Performance Optimization`

---

## 14. 和已有文档如何搭配阅读

- 想先看整体结构：读 `docs/architecture.md`
- 想看接口和协议字段：读 `docs/api.md`
- 想看技术栈清单：读 `docs/tech-stack.md`
- 想看优化和难点：读 `docs/challenges.md`
- 想看数据：读 `docs/perf-report.md`
- 想练面试表达：读 `docs/interview-questions.md`
- 想快速建立完整认知：先读本文
