# Local Agent (Qwen2.5-7B-Instruct)

一个只依赖**本地模型**的极简可扩展 AI Agent。默认使用：

- LLM: `/mnt/sda/tmh/self_build/Qwen2.5-7B-Instruct`

> 只选择一个本地模型（Qwen）作为核心推理引擎，保证简单与可扩展性。你可以很容易地接入更多工具或替换模型。

## ✨ 功能
- ReAct 风格推理 + 工具调用（自定义注册制）
- 对话记忆（短期 BufferMemory）
- 内置两个安全工具：
  - `calculator`：安全计算器（受限表达式）
  - `fs_search`：本地文件系统关键词搜索（返回文件路径片段）

## 📦 目录
```
local_agent_qwen/
├── README.md
├── requirements.txt
├── config.yaml
├── cli.py
├── agent/
│   ├── __init__.py
│   ├── llm.py
│   ├── schema.py
│   ├── memory.py
│   └── agent.py
└── tools/
    ├── __init__.py
    ├── calculator.py
    └── fs_search.py
```

## 🔧 安装
```bash
pip install -r requirements.txt
```

> 需要 GPU 环境更顺畅地运行 7B 模型（也支持 CPU，但会较慢）。

## ⚙️ 配置
编辑 `config.yaml`：
```yaml
model_path: /mnt/sda/tmh/self_build/Qwen2.5-7B-Instruct
device: auto         # "auto" | "cuda" | "cpu"
dtype: auto          # "auto" | "float16" | "bfloat16" | "float32"
max_tokens: 1024
temperature: 0.7
top_p: 0.95
```

## ▶️ 运行
### 交互式聊天
```bash
python cli.py chat -c "你是谁？"
```

### 多轮对话（交互模式）
```bash
python cli.py chat
# 然后逐条输入你的问题（输入 exit 结束）
```

### 自定义工具
在 `tools/` 目录新建一个 Python 文件，实现 `register(tool_registry)` 函数并将你的工具以函数形式注册：
```python
def my_tool(x: int) -> int:
    '''加 1 示例。'''
    return x + 1

def register(tool_registry):
    tool_registry.register(
        name="my_tool",
        func=my_tool,
        description="对整数加 1",
        schema={"type": "object", "properties": {"x": {"type": "integer"}}, "required": ["x"]}
    )
```

在 `cli.py` 中会自动发现并加载所有 `tools/*.py`（忽略下划线开头的文件）。

## 🧠 提示词格式（ReAct）
Agent 引导模型以如下格式调用工具：
```
思考: <模型的推理>
Tool: <工具名>
Args: {"key": "value"}

# 工具运行后，系统会注入：
Observation: <工具输出>

# 模型继续：
思考: ...
最终: <给用户的最终回答>
```
当模型输出以 `最终:` 开头时，Agent 会停止并把内容返回给用户。

## 🔁 开发建议
- 想更强的搜索/检索能力，可以接入你自己的检索工具；
- 如果后续要支持函数调用（JSON Schema）或 OpenAI 风格 tools，这个架构也很容易扩展；
- 对算子安全性敏感时，可以将工具放到沙盒环境（例如子进程 + 限制资源）。

---

© 2025 musha - local agent minimal scaffolding


## 新增工具：本地文件读/写

两个新工具已集成到 `tools/` 下：

- `read_local_file` —— 读取本地文件（文本/二进制，支持偏移+限长分页）
  ```
  Tool: read_local_file
  Args: {"path": "/tmp/demo.txt", "as_text": true, "start": 0, "max_bytes": 1024}
  ```

- `write_local_file` —— 写入本地文件（文本/二进制，支持base64、覆盖/追加）
  ```
  Tool: write_local_file
  Args: {"path": "/tmp/demo.txt", "content": "你好，本地Agent！", "as_text": true, "append": false}
  ```

二进制写入示例（base64）：
```
Tool: write_local_file
Args: {
  "path": "/tmp/pic.bin",
  "content": "<BASE64>",
  "as_text": false,
  "binary_is_base64": true
}
```

二进制读取（仅回传十六进制前缀，避免脏字符污染对话）
```
Tool: read_local_file
Args: {"path": "/tmp/pic.bin", "as_text": false, "start": 0, "max_bytes": 512}
```
