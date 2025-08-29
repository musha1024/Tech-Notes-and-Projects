# C++ MySQL + Redis 微服务框架（Win/Linux 通用，含优化与防护）

> 一个**可直接编译运行**的 C++ 后端服务骨架：以 **MySQL** 为主存、以 **Redis** 为缓存，提供 **插入** 与 **查询** 两个核心接口。
> 附带：**详细注释**、**端到端 I/O 说明**、**可扩展建议**、**SQL 性能优化清单**、**SQL 注入防护基线**。

---

## 目录
- [1. 架构与数据流](#1-架构与数据流)
- [2. API 设计（输入 / 输出 / 错误码）](#2-api-设计输入--输出--错误码)
- [3. 数据模型与表结构](#3-数据模型与表结构)
- [4. 构建与运行（Windows / Linux）](#4-构建与运行windows--linux)
- [5. 配置说明](#5-配置说明)
- [6. 缓存策略（Redis）与一致性](#6-缓存策略redis与一致性)
- [7. SQL 性能优化要点（实践清单）](#7-sql-性能优化要点实践清单)
- [8. SQL 注入防护与安全基线](#8-sql-注入防护与安全基线)
- [9. 可扩展点与演进路线](#9-可扩展点与演进路线)
- [10. 代码结构与关键模块](#10-代码结构与关键模块)
- [11. 测试与验证](#11-测试与验证)

---

## 1. 架构与数据流

```
Client
  ├── POST /user {name, age}
  └── GET  /user?id=...
         │
         ▼
   HTTP Server (cpp-httplib)
         │
         ▼
   Controller（路由处理）
         │
         ▼
   DAO（UserDao）
   ├── MySQL（主存：插入 / 回源查询）
   └── Redis（缓存：优先读 / 回填；写入后回写缓存）
```

- **读路径**：优先读 Redis → 未命中回源 MySQL → 命中则回填 Redis（缓存旁路 Cache-Aside）。
- **写路径**：写 MySQL 成功后，写/刷新对应 Redis Key（提升读后命中率）。
- **连接复用**：MySQL 使用**连接池**；Redis 可按需做连接池（此骨架使用单连接，易于替换）。

---

## 2. API 设计（输入 / 输出 / 错误码）

### 2.1 插入用户
- **Method / Path**：`POST /user`
- **Request-Body**（JSON）：
  ```json
  { "name": "Alice", "age": 23 }
  ```
- **Response-Body**（JSON 成功）：
  ```json
  { "id": 1 }
  ```
- **错误码**：
  - `400`：参数缺失或类型错误（例如 age 不是整数）
  - `500`：插入失败（数据库错误）

### 2.2 查询用户
- **Method / Path**：`GET /user?id=<int64>`
- **Query 参数**：`id`（必填，整数）
- **Response-Body**（JSON 成功）：
  ```json
  { "id": 1, "name": "Alice", "age": 23 }
  ```
- **错误码**：
  - `400`：`id` 缺失或非法
  - `404`：未找到记录

> 统一返回 `application/json`，错误响应包含 `"error"` 字段。

---

## 3. 数据模型与表结构

- **实体：User**
  - 字段：`id BIGINT AUTO_INCREMENT PRIMARY KEY`, `name VARCHAR(64) NOT NULL`, `age INT NOT NULL`, `created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP`
  - 建议索引：`idx_users_age(age)`（示例，便于按年龄过滤/排序）

**建表 SQL**：
```sql
CREATE DATABASE demo CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;
USE demo;

CREATE TABLE users (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  name VARCHAR(64) NOT NULL,
  age INT NOT NULL,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB;

CREATE INDEX idx_users_age ON users(age);
```

---

## 4. 构建与运行（Windows / Linux）

### 4.1 依赖
- `cpp-httplib`（HTTP 服务）
- `nlohmann/json`（JSON）
- `mysql-connector-cpp`（MySQL C++ 驱动）
- `redis-plus-plus`（可选）

**推荐 vcpkg 安装**：
- Windows（MSVC）
  ```powershell
  git clone https://github.com/microsoft/vcpkg
  .cpkgootstrap-vcpkg.bat
  .cpkgcpkg install httplib nlohmann-json unofficial-mysql-connector-cpp redis-plus-plus:x64-windows
  cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="%CD%/vcpkg/scripts/buildsystems/vcpkg.cmake" -DWITH_REDIS=ON -DCMAKE_BUILD_TYPE=Release
  cmake --build build --config Release
  ```
- Linux（GCC/Clang）
  ```bash
  git clone https://github.com/microsoft/vcpkg
  ./vcpkg/bootstrap-vcpkg.sh
  ./vcpkg/vcpkg install httplib nlohmann-json unofficial-mysql-connector-cpp redis-plus-plus
  cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=$PWD/vcpkg/scripts/buildsystems/vcpkg.cmake -DWITH_REDIS=ON -DCMAKE_BUILD_TYPE=Release
  cmake --build build -j
  ```

> 如不需要 Redis，可加入 `-DWITH_REDIS=OFF` 构建。

### 4.2 运行
```bash
./build/cpp_kv_service ./config.json
```
健康检测：`GET /health` → `ok`。

---

## 5. 配置说明

`config.json`：
```json
{
  "server": { "host": "0.0.0.0", "port": 8080 },
  "mysql": {
    "host": "127.0.0.1",
    "port": 3306,
    "user": "app_user",
    "password": "******",
    "database": "demo",
    "pool_size": 5
  },
  "redis": {
    "host": "127.0.0.1",
    "port": 6379,
    "password": "",
    "db": 0,
    "ttl_seconds": 300
  }
}
```
- **数据库账号**建议使用**最小权限**：仅限于 `SELECT/INSERT/UPDATE` 等所需权限。
- `pool_size` 根据 QPS、MySQL 最大连接数进行压测后调整。
- `ttl_seconds` 为缓存过期秒数，可按热点程度区分。

---

## 6. 缓存策略（Redis）与一致性

- **模式**：Cache-Aside（旁路缓存）
  - **读**：先查缓存 → 未命中查库 → 回填缓存
  - **写**：写库成功后，**更新/删除缓存**（本项目为更新回填）
- **穿透**：对不存在的 id，可缓存**空值**短 TTL；或引入**布隆过滤器**（集中过滤非法 id）。
- **击穿**：热点 Key 到期瞬间并发回源，可加**互斥锁**（分布式锁 SETNX + 合理过期）或**逻辑过期**。
- **雪崩**：大量 Key 同时过期，给 TTL 增加**随机抖动**（±10~20%）。
- **一致性**：对一致性要求高的写操作，可采用 **延迟双删**（写库后删缓存→短暂 sleep→再次删）。

---

## 7. SQL 性能优化要点（实践清单）

1) **表结构与字段类型**  
   - 精确选择类型：`BIGINT` 自增主键；`VARCHAR(64)` 而非过大长度；`TIMESTAMP` 自动时间。  
   - 合理字符集与校对规则：`utf8mb4` + `utf8mb4_0900_ai_ci`。

2) **索引设计**  
   - 高选择性列建索引；尽量覆盖常用查询的过滤/排序字段。  
   - 组合索引遵守**最左前缀**；避免在列上使用函数（会导致索引失效）。  
   - 避免**过多索引**（维护成本高，写入变慢）。

3) **SQL 写法**  
   - **避免 `SELECT *`**，按需列出字段。  
   - WHERE 条件**可 SARG**（可被索引使用）：避免 `WHERE func(col)=...`、`col + 1 = ...` 等。  
   - 大分页优化：`WHERE id > ? ORDER BY id LIMIT N` 或 `keyset pagination`。  
   - 需要批量插入时，使用批量 `INSERT`（多值）或事务包裹多条 `INSERT`。

4) **连接池与事务**  
   - 连接池大小把控：过大反而争用；压测决定。  
   - 只在必要范围内开启事务；**合理隔离级别**（一般 `READ COMMITTED`/`REPEATABLE READ`）。

5) **EXPLAIN 诊断（常看字段）**  
   - `type`（访问类型）：`const`/`ref`/`range`/`index`/`ALL`（从好到差），尽量避免 `ALL` 全表扫。  
   - `key`/`key_len`（用到的索引及长度）  
   - `rows`（预估扫描行数，越小越好）  
   - `Extra`：`Using index`（覆盖索引好）、`Using where`（常见）、`Using filesort`/`Using temporary`（注意）。

6) **慢查询与可观测**  
   - 开启慢查询日志（`long_query_time` 合理设置），按 TopN 聚合定位问题。  
   - 配合 `performance_schema` 观察等待事件、IO。

7) **分区/分表与冷热分离（演进）**  
   - 超大表可**分区**或**按范围/哈希分表**；历史数据**归档**。  
   - 读写分离：从库只读，提升查询吞吐。

---

## 8. SQL 注入防护与安全基线

- **强制使用预编译语句（PreparedStatement）**（本项目已全部采用 `?` 占位）  
  ✅ 输入永不拼接到 SQL 字符串中。  
- **输入校验**：后端再次校验类型与范围（例如 `age >= 0`）。  
- **最小权限**：APP 账号仅授予必要权限；禁止 `DROP/ALTER` 等破坏性权限。  
- **错误处理**：屏蔽数据库底层错误细节，仅返回通用错误信息；详细错误写入日志。  
- **审计与告警**：审计表或日志系统记录敏感操作并告警。  
- **LIKE 查询**如需支持通配，可使用参数化并对 `%`、`_` 做转义 + `ESCAPE`。  
- **Secrets 管理**：生产环境不要把明文密码提交到仓库；使用环境变量/密钥管理器。

**示例（已在代码中实现）**：
```cpp
std::unique_ptr<sql::PreparedStatement> stmt(conn->prepareStatement(
  "INSERT INTO users(name, age) VALUES(?, ?)"));
stmt->setString(1, name);   // ✅ 不拼接
stmt->setInt(2, age);
stmt->executeUpdate();
```

---

## 9. 可扩展点与演进路线

- **水平扩展**：
  - 增加更多实体（如 `orders`）：按 `model/*` / `dao/*` 模板复制扩展；在 `main.cpp` 注册路由。  
  - 加入**鉴权**（JWT/OAuth2）、**限流**、**CORS**、**访问日志**、**Trace（OpenTelemetry）**。  
  - Redis 连接池与 Pipeline；高并发场景下热点 Key 保护。

- **一致性与高可用**：
  - MySQL 主从 + 哨兵/Proxy；Redis 主从 + 哨兵或 Cluster。  
  - 引入**分布式锁**（SETNX + 过期 + 唯一 token）用于强一致关键路径。

- **部署工程化**：
  - Dockerfile + docker-compose（MySQL/Redis/服务）一键起；K8s StatefulSet + Service。  
  - 灰度发布与健康检查，/health 探针已提供。

---

## 10. 代码结构与关键模块

```
cpp-microservice/
├─ CMakeLists.txt
├─ config.json
├─ README.md
├─ include/
│  ├─ app/config.h          # 配置结构体与加载接口
│  ├─ app/logger.h          # 简易日志工具
│  ├─ db/mysql_pool.h       # MySQL 连接池（带 RAII Guard）
│  ├─ db/redis_client.h     # Redis 客户端（可选编译）
│  ├─ model/user.h          # User 数据模型（JSON 序列化）
│  ├─ model/user_dao.h      # User DAO（插入/查询，带缓存）
│  └─ util/strutil.h        # 小工具：字符串转数字
└─ src/
   ├─ main.cpp              # 路由注册、依赖初始化、HTTP 服务
   ├─ app/config.cpp        # JSON 配置加载实现
   ├─ db/mysql_pool.cpp     # MySQL 连接池实现
   ├─ db/redis_client.cpp   # Redis 客户端实现
   └─ model/user_dao.cpp    # DAO 实现（PreparedStatement + Cache-Aside）
```

---

## 11. 测试与验证

### 快速冒烟：
```bash
# 插入
curl -X POST http://127.0.0.1:8080/user -H 'Content-Type: application/json'   -d '{"name":"Alice","age":23}'

# 查询（优先命中缓存，否则回源数据库并回填）
curl "http://127.0.0.1:8080/user?id=1"
```

### 压测建议：
- 使用 `wrk`/`ab` 对 `/user` 接口做并发读/写测试，观察 QPS/TP99。
- 调整 `pool_size`、Redis TTL、索引设计，并复核 `EXPLAIN` 与慢查询日志。
