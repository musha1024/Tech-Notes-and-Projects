from typing import Dict, Any, Optional
import importlib
import pkgutil
from pathlib import Path
import json
from agent.schema import ToolRegistry
from agent.memory import BufferMemory

SYSTEM_PROMPT = """你是一个严谨的助手。你可以选择：
1) 直接回答用户问题；
2) 当需要外部信息或计算时，按如下格式调用工具：

思考: <你的简短推理>
Tool: <工具名>
Args: {"k": "v"}

收到 Observation 后继续思考，直到你能给出最终答案：
最终: <给用户的最终答案>
或
Final: <给用户的最终答案>

严格要求：
- 每一轮要么给出 Tool/Args，要么给出 最终:/Final:，不要两者混在同一轮。
- Args 必须是合法 JSON。
- 工具名只能从可用工具列表中选择。

可用工具示例：
- read_local_file(path, as_text=True, start=0, max_bytes=1024)
- write_local_file(path, content, as_text=True, append=False)

示例1：创建并写入文本
思考: 需要创建文件并写入文本
Tool: write_local_file
Args: {"path": "/tmp/hello.txt", "content": "hello world", "as_text": true, "append": false}

(收到 Observation 后)
思考: 文件已写入，给出确认信息
最终: 已创建 /tmp/hello.txt 并写入 11 个字符。

示例2：读取文本文件前 100 字节
思考: 需要读取文本
Tool: read_local_file
Args: {"path": "/tmp/hello.txt", "as_text": true, "start": 0, "max_bytes": 100}

(收到 Observation 后)
Final: 读取成功，已展示片段。
"""

def discover_tools(registry: ToolRegistry):
    pkg_path = Path(__file__).parent.parent / "tools"
    for modinfo in pkgutil.iter_modules([str(pkg_path)]):
        name = modinfo.name
        if name.startswith("_"):
            continue
        module = importlib.import_module(f"tools.{name}")
        if hasattr(module, "register"):
            module.register(registry)

class LocalAgent:
    def __init__(self, llm, memory: Optional[BufferMemory] = None, max_steps: int = 6):
        self.llm = llm
        self.memory = memory or BufferMemory(capacity=10)
        self.registry = ToolRegistry()
        discover_tools(self.registry)
        self.max_steps = max_steps

    def build_prompt(self, user_input: str, scratchpad: str = "") -> str:
        mem_text = self.memory.as_text()
        tool_desc = "\n".join([
            f"- {spec.name}: {spec.description}. Args schema: {json.dumps(spec.schema, ensure_ascii=False)}"
            for spec in self.registry.tools.values()
        ])
        prompt = f"""{SYSTEM_PROMPT}

可用工具：
{tool_desc}

对话记忆：
{mem_text if mem_text else "(空)"}

现在的用户问题：{user_input}

请严格使用上述格式进行输出。
{scratchpad}"""
        return prompt

    def parse_tool_call(self, text: str):
        tool_line = None
        args_line = None
        for line in text.splitlines():
            if line.strip().startswith("Tool:"):
                tool_line = line.strip()
            if line.strip().startswith("Args:"):
                args_line = line.strip()
        if tool_line and args_line:
            tool_name = tool_line.split("Tool:", 1)[1].strip()
            args_str = args_line.split("Args:", 1)[1].strip()
            try:
                args = json.loads(args_str)
            except json.JSONDecodeError:
                args = {}
            return tool_name, args
        return None, None

    def _extract_final(self, text: str) -> Optional[str]:
        """提取最后一个 Final/最终 答案"""
        for tag in ["最终:", "Final:"]:
            if tag in text:
                return text.split(tag)[-1].strip()
        return None

    def chat(self, user_input: str) -> str:
        scratchpad = ""
        for step in range(self.max_steps):
            prompt = self.build_prompt(user_input, scratchpad)
            output = self.llm.generate(prompt).strip()

            # 提取 Final/最终 答案
            final = self._extract_final(output)
            if final is not None:
                self.memory.append(user_input, final)
                return final

            # 工具调用
            tool_name, args = self.parse_tool_call(output)
            if tool_name and tool_name in self.registry.tools:
                spec = self.registry.get(tool_name)
                try:
                    observation = spec.func(**args)
                except TypeError as e:
                    observation = f"[参数错误] {e}"
                except Exception as e:
                    observation = f"[运行错误] {e}"
                scratchpad += f"\n{output}\nObservation: {observation}\n"
            else:
                # 提醒模型格式
                scratchpad += f"\n(提示：请按规范输出 Tool/Args 或 Final:/最终: ...)\n"

        # 超过最大步数，直接返回
        self.memory.append(user_input, output)
        return output
