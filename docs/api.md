# MiniRPC API 与协议说明

## 1. 协议头

`FrameHeader` 位于 `include/minirpc/protocol.h`。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `magic` | `uint32` | 协议魔数，当前为 `MRPC` |
| `version` | `uint8` | 协议版本 |
| `message_type` | `uint8` | 请求 / 响应 |
| `codec` | `uint8` | 编解码类型，当前为 `MiniBinary` |
| `flags` | `uint8` | 预留扩展位 |
| `request_id` | `uint64` | 请求唯一标识 |
| `body_length` | `uint32` | 包体字节数 |

## 2. 请求体

请求包体包含：

- `service`
- `method`
- `auth_token`
- `deadline_unix_ms`
- `payload`

## 3. 响应体

响应包体包含：

- `status`
- `message`
- `payload`

## 4. 运行时 API

### `mrpc::RpcServer`

- `RegisterMethod(service, method, handler)`：注册服务方法
- `Start()`：启动监听
- `Stop()`：停止服务
- `bound_port()`：读取真实监听端口

### `mrpc::RpcChannel`

- `Call(service, method, request_payload, response_payload, timeout_ms)`：发起同步 RPC 调用
- `Reset()`：主动断开连接

### `mrpc::StaticRegistry`

- `LoadFromFile(path)`：加载静态注册发现配置
- `Find(service_name)`：根据服务名获取地址

## 5. 生成代码 API

以 `CalculatorService` 为例，生成：

- `demo::AddRequest / AddResponse` 等消息类型
- `demo::CalculatorService` 抽象基类
- `demo::RegisterCalculatorService(...)`
- `demo::CalculatorServiceClient`

客户端调用示例：

```cpp
mrpc::RpcChannel channel(endpoint, "mini-secret");
demo::CalculatorServiceClient client(&channel);

demo::AddRequest request;
request.lhs = 10;
request.rhs = 32;

demo::AddResponse response;
auto status = client.Add(request, &response, 1000);
```

服务端注册示例：

```cpp
mrpc::RpcServer server(options);
demo::DemoCalculatorService service;
demo::RegisterCalculatorService(&server, &service);
server.Start();
```

## 6. 当前 IDL 能力

支持的语法：

- `package`
- `message`
- `service`
- `rpc ... returns (...)`

支持字段类型：

- `bool`
- `int32`
- `int64`
- `uint32`
- `uint64`
- `double`
- `string`

## 7. 错误码

| 错误码 | 含义 |
| --- | --- |
| `kOk` | 成功 |
| `kUnknownService` | 服务不存在 |
| `kUnknownMethod` | 方法不存在 |
| `kBadRequest` | 请求解码失败 / 非法 |
| `kTimeout` | 超时 |
| `kAuthFailed` | 鉴权失败 |
| `kBusy` | 线程池队列已满，快速拒绝 |
| `kNetworkError` | 网络错误 |
| `kSerializeError` | 序列化 / 反序列化错误 |
| `kInternalError` | 其他内部错误 |
