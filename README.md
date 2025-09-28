# Tech-Notes-and-Projects

## 📌 简介
本仓库用于 **个人学习与实践记录**，主要包含：
- 常用开发模板与环境配置
- C++/Python 微服务与分布式实验
- LLM / RAG / Agent 相关实现
- 学习过程中的常见问题与解决方案

目标是逐步沉淀成一套 **可复用的知识库与项目模板**。

---

## 📂 内容目录

### 1️⃣ 基础工具与模板
- **CMake 模板**：模块化项目组织，常用编译/调试参数
- **Git 指南**：clone / commit / push / pull，分支管理，冲突解决与代理配置

### 2️⃣ 数据库与缓存微服务
- **MySQL 微服务**
  - C++ 封装 MySQL 连接池
  - 基础 CRUD 接口
- **Redis 微服务**
  - Redis 客户端与缓存读写示例
  - Write-through / Cache-aside 策略
  - 与 MySQL 组合的缓存更新逻辑

### 3️⃣ C++ 网络与分布式
- **RPC 框架雏形** (`/RPC/`)
  - 基于 Socket/HTTP 的调用流程
- **C++ 微服务集合** (`/cpp-microservice/`)
  - `cpp_chat_room`：基于 TCP/epoll 的多客户端聊天室（支持广播）
  - `mysql_microservice_modular`：C++ MySQL 连接池
  - `redis_microservice`：Redis 客户端与缓存策略
  - `cpp-redis-mysql-ms`：Redis + MySQL 组合示例

### 4️⃣ LLM / RAG / Agent
- **RAG 本地实现 (`rag_qwen_bge_compare`)**
  - Qwen vs BGE 模型在 RAG 中的对比
  - 向量检索 + 召回 + 生成完整流程
  - Base 模式 vs RAG 模式对比输出
- **Local Agent (ChatGPT API)**
  - 极简 ReAct 风格 Agent 框架
  - 内置工具：计算器 / 本地文件搜索 / 文件读写
  - 支持对话记忆与工具扩展

### 5️⃣ 部署与实验环境
- **llama.cpp with Docker**
  - 从 Docker 拉取基础镜像
  - 配置 CUDA / GPU 加速
  - 编译与运行 llama.cpp 推理

---

