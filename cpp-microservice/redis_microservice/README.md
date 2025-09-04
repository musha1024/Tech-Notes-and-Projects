# Redis 微服务（C++，零第三方依赖）

一个极简但可用的 **Redis 微服务**：提供 HTTP/JSON 接口封装常见 Redis 操作（GET/SET/DEL/INCR/EXISTS），
用 **原生 POSIX socket** 写了小型 HTTP Server，内部 Redis 客户端基于 **RESP 协议** 自实现，
**参数化发送**，从源头预防“命令拼接式注入”。

> 目标：能开箱即用、易读易改、适合作为面试/学习与小规模生产的脚手架。

---

## ✨ 功能清单

- HTTP 路由：
  - `GET /ping`：健康检查
  - `GET /get?key=K`
  - `POST /set`：`{"key":"K","value":"V","ttl":60}`（`ttl`可选，秒）
  - `DELETE /del?key=K`
  - `POST /incr`：`{"key":"K","by":2}`（`by`可选，默认 1）
  - `GET /exists?key=K`
- Redis 连接：`AUTH` 可选（通过 `--redis-password` 或 `REDIS_PASSWORD` 环境变量）
- **防注入**：
  - Redis 命令通过 **RESP** 参数化编码（`*N\r\n$len\r\narg\r\n...`）发送，**不会因包含空格/换行而拆分参数**。
  - 额外的 **key 白名单校验**（`^[A-Za-z0-9:_-]{1,128}$`），限制 value 大小（默认 1MB）。
- 可靠性小措施：
  - HTTP 头与 body 设定**上限**（1MB header / 4MB body / 16MB Redis 回复）
  - 错误码与 JSON 统一返回
- 依赖：**仅 C++17 + POSIX sockets**（Linux/macOS；Windows 可用 WSL）

---

## 🧱 目录结构

```
redis_microservice/
├── CMakeLists.txt
├── include/
│   └── redis_client.hpp        # RESP 安全的 Redis 客户端（零依赖）
└── src/
    └── main.cpp                # 迷你 HTTP 服务 + 路由 + 业务逻辑
```

---

## ⚙️ 构建与运行

### 1) 构建

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
```

可选：开启地址/未定义行为检测（开发期好用）

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON
cmake --build . -j
```

### 2) 启动服务

```bash
# 默认监听 0.0.0.0:8080，连接 127.0.0.1:6379（可覆盖）
./redis_microservice \
  --port 8080 \
  --redis-host 127.0.0.1 \
  --redis-port 6379 \
  --redis-password "" \
  --redis-timeout-ms 2000
```

也可用环境变量：`PORT`、`REDIS_HOST`、`REDIS_PORT`、`REDIS_PASSWORD`。

---

## 📡 API 示例（curl）

```bash
# 健康检查
curl -i http://127.0.0.1:8080/ping

# 写入（带 TTL）
curl -i -X POST http://127.0.0.1:8080/set \
  -H "Content-Type: application/json" \
  -d '{"key":"user:1","value":"Alice","ttl":60}'

# 读取
curl -i "http://127.0.0.1:8080/get?key=user:1"

# 自增
curl -i -X POST http://127.0.0.1:8080/incr \
  -H "Content-Type: application/json" \
  -d '{"key":"cnt","by":2}'

# 判断是否存在
curl -i "http://127.0.0.1:8080/exists?key=cnt"

# 删除
curl -i -X DELETE "http://127.0.0.1:8080/del?key=user:1"
```

返回示例：

```json
{"ok":true}
{"key":"user:1","value":"Alice"}
{"key":"user:1","value":null}
{"new":42}
{"exists":true}
{"deleted":1}
```

---

## 🔐 安全设计（防注入与输入约束）

1. **RESP 参数化命令**（核心）：  
   本项目所有 Redis 调用都走 `build_resp_array()` 构造 `*N/$len/arg` 的 **长度前缀协议**，
   Redis 以**字节长度**区分参数，**不**会因空格、换行、特殊字符而“拆分命令”。  
   这天然避免了“字符串拼接式命令注入”。

2. **Key 白名单**：`^[A-Za-z0-9:_-]{1,128}$`，拒绝异常/过长 key。

3. **值与报文上限**：
   - HTTP Header ≤ 1MB，HTTP Body ≤ 4MB
   - Redis 回复 ≤ 16MB
   - Value ≤ 1MB（可在 `main.cpp` 中调整）

4. **错误码**：统一 JSON 返回，避免泄露内部堆栈。

> 如需更强的租户隔离，可在 key 前统一加租户前缀（如 `tenant123:`），并在服务层做 ACL。

---

## 🧩 可扩展建议

- **命令扩展**：在 `RedisClient` 中按同样模式添加 `HSET/HGET/MGET/SCAN/...`。  
- **路由**：为每个命令加独立处理器，或引入简单路由表与中间件（限流、鉴权、审计）。
- **并发**：当前是“每连接一线程”，小流量够用；高并发可改线程池或 epoll。  
- **JSON 库**：目前请求 JSON 用 `std::regex` 做了“足够简单”的解析，若要更复杂可集成 `nlohmann/json`（单头文件）。
- **Windows**：建议 WSL。若要原生 Windows，需切换到 Winsock（`WSAStartup/WSACleanup`）。

---

## 🛠 常见问题

- **编译报错找不到 `<.../netinet/...>`**：你在 Windows。请使用 WSL 或改写为 Winsock。  
- **连接 Redis 失败**：确认 `redis-server` 已启动、`--redis-host/port` 正确、防火墙放行。  
- **返回 413**：请求值过大（默认 1MB）；可在代码中调整。

---

## 📜 许可

MIT。随意使用/改造，欢迎 PR。

---

## 📦 快速复用（模板）

你可以把 `include/redis_client.hpp` 单拷到自己的项目里，直接使用 `RedisClient`：

```cpp
RedisClientConfig cfg;
cfg.host = "127.0.0.1";
cfg.port = 6379;
RedisClient cli(cfg);
cli.connect();
cli.set("k", "v");
auto v = cli.get("k"); // std::optional<std::string>
```
