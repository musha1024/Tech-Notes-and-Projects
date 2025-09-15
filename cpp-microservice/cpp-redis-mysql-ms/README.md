# C++ Redis + MySQL 微服务（支持建表与按表操作）

- **读优先 Redis**，未命中回源 **MySQL** 并回填缓存（Read-Through / Cache-Aside）。
- **写默认 Write-Through**：先写 MySQL，再 `SETEX` Redis（可带 TTL）。
- **防注入**：MySQL 全部使用预处理语句；表名严格校验 + 反引号包裹（标识符安全）。
- **零外部 HTTP 框架**，仅依赖 `hiredis` 与 `libmysqlclient`。

## 依赖安装（Ubuntu/Debian）
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libhiredis-dev libmysqlclient-dev
```

## 构建与运行
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j

# 启动（示例）
./redis_mysql_ms \
  --port 8081 \
  --redis-host 127.0.0.1 --redis-port 6379 --redis-db 0 \
  --mysql-host 127.0.0.1 --mysql-port 3306 \
  --mysql-user root --mysql-password "" --mysql-db test \
  --mysql-pool-size 4 --default-ttl 300
```
> 程序会确保默认表 `kv_store` 存在。

## API
- 健康检查：`GET /health`
- 创建表：`POST /table/create`，Body: `{"name":"kv_tenant1"}`
- 查询：`GET /get?key=K[&table=kv_tenant1]`
- 新增：`POST /set[?table=...]`，Body: `{"key":"K","value":"V","ttl":300}`
- 更新：`POST /update[?table=...]`，Body 同上
- 删除：`DELETE /del?key=K[&table=...]`

> 未指定 `table` 时默认使用 `kv_store`。

## 更新策略与一致性
- **读**：Redis 命中则返回；未命中 -> MySQL -> 命中回填 Redis（`SETEX`）。
- **写**：Write-Through（先 MySQL 再 Redis）；若 Redis 刷新失败，读路径仍能回源并回填。
- **删**：先删 MySQL，再 `DEL` Redis。
- **TTL**：默认 `--default-ttl`，可 per-key 覆盖；可加随机抖动与空值缓存以防穿透/雪崩（示例未启用）。

## Python 测试脚本
```bash
pip install requests
python python_test_client.py
```
脚本包含：基本 CRUD、TTL 过期后的回源，以及**建表 + 指定表操作**示例。
