
import os
from typing import List, Dict
from openai import OpenAI

class OpenAILLM:
    """
    OpenAI Chat Completions 后端，兼容 LocalAgent 的简单接口：
    - generate(messages) -> str
    其中 messages 为 [{role, content}, ...]
    """
    def __init__(self, model: str = "gpt-4o-mini", temperature: float = 0.2, top_p: float = 0.95, max_tokens: int = 1024, api_base: str | None = None, api_key: str | None = None):
        api_key = api_key or os.getenv("OPENAI_API_KEY")
        if not api_key:
            raise RuntimeError("缺少 OPENAI_API_KEY 环境变量或未传入 api_key")
        client_kwargs = {"api_key": api_key}
        if api_base:
            client_kwargs["base_url"] = api_base
        self.client = OpenAI(**client_kwargs)
        self.model = model
        self.temperature = temperature
        self.top_p = top_p
        self.max_tokens = max_tokens

    def generate(self, messages: List[Dict[str, str]]) -> str:
        # OpenAI SDK 1.x: client.chat.completions.create
        resp = self.client.chat.completions.create(
            model=self.model,
            messages=messages,
            temperature=self.temperature,
            top_p=self.top_p,
            max_tokens=self.max_tokens,
        )
        return resp.choices[0].message.content or ""
