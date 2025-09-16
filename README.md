# Tech-Notes-and-Projects

## 📌 项目简介
本仓库用于 **个人学习与实践记录**，主要包含：
- 学习过程中整理的 **模板代码**
- 常见问题与解决方法
- 一些开发与实验性的实现

同时借助 **大语言模型 (LLM)** 辅助做规划与优化，使内容更系统化和可复用。仓库内容会持续更新。

---

## 📂 当前内容

### 🔧 CMake 常用操作
- 常见 CMakeLists.txt 模板  
- 模块化项目组织方式  
- 常用编译 / 调试参数示例  

### 🗄️ MySQL 微服务模板
- 使用 C++ 封装的 MySQL 连接池  
- 简单接口  
- 基础的读写、更新、删除操作流程  

### ⚡ Redis 微服务模板
- Redis 客户端与缓存读写示例  
- Write-through / Cache-aside 策略演示  
- 与 MySQL 组合的缓存更新逻辑  

### 🌱 Git 常用指令
- 基础操作（clone / commit / push / pull）  
- 分支管理与合并示例  
- 常用问题与解决办法（如冲突处理、代理配置）  

### 🔗 RPC 框架
- 简单自研 RPC 框架雏形  
- 基于 Socket/HTTP 的调用流程  
- 与微服务示例结合  

### 🐳 llama.cpp 基于 Docker 的部署
- 从 Docker 拉取基础镜像  
- 配置 CUDA / GPU 加速环境  
- 编译与运行 llama.cpp 推理的流程  

### 📖 RAG 本地实现 (`rag_qwen_bge_compare`)
- 对比 **Qwen 模型** 与 **BGE 模型** 在 RAG（Retrieval-Augmented Generation）中的效果  
- 实现向量检索、召回、生成回答的完整流程  
- 提供 **Base 模式 vs RAG 模式** 的对比输出  
- 可扩展为本地知识库问答系统  

---
### 🤖 Local Agent (Qwen2.5-7B-Instruct)
- 基于本地 **Qwen2.5-7B-Instruct** 模型实现的极简 Agent 框架  
- 支持 **ReAct 风格** 推理与工具调用  
- 内置工具：
  - `calculator`（安全计算器）
  - `fs_search`（本地文件系统关键词检索）
- 提供 **对话记忆** (BufferMemory)  
- 可扩展：只需在 `tools/` 目录添加 Python 文件并实现 `register()` 即可集成新工具  
- 输出清理：兼容 `Final:` / `最终:`，确保只返回最终答案而非冗长生成过程
