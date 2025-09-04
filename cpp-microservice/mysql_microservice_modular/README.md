# C++ MySQL 微服务（模块化头文件版，防注入）

更“标准”的目录结构：头/源分离、职责清晰、易扩展。功能同前（REST + MySQL 预处理语句）。

## 结构
```
mysql_microservice_modular/
├── CMakeLists.txt
├── include/
│   └── service/
│       ├── config.hpp        # 配置解析
│       ├── handlers.hpp      # 路由与HTTP处理
│       └── mysql_pool.hpp    # 连接池与SQL封装
├── src/
│   ├── config.cpp
│   ├── handlers.cpp
│   ├── main.cpp
│   └── mysql_pool.cpp
└── tests/
    └── test_client.py
```

## 构建与运行
与上一版一致：
```bash
mkdir build && cd build
cmake ..
cmake --build . -j
./mysql_microservice --port 8081 --mysql-host 127.0.0.1 --mysql-port 3306 \
  --mysql-user root --mysql-password "" --mysql-db test --pool-size 8
```

首次初始化（演示环境）：
```bash
curl -X POST http://127.0.0.1:8081/admin/init
```

## 安全与规范
- 头/源分离、公共接口在 `include/service/*.hpp`
- 防注入：**全部**数据库访问都用 `mysql_stmt_*` 预处理
- 字符集统一 `utf8mb4`
- 参数校验（长度、limit/offset）
- 便于后续加：鉴权、限流、日志、指标、事务等
