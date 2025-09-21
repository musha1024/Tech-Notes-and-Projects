from collections import deque
from typing import List, Tuple

class BufferMemory:
    """简单的对话缓存，最多保存 N 轮（user+assistant 记为一轮）。"""
    def __init__(self, capacity: int = 10):
        self.capacity = capacity
        self.buffer: deque[tuple[str, str]] = deque()

    def append(self, user: str, assistant: str):
        self.buffer.append((user, assistant))
        while len(self.buffer) > self.capacity:
            self.buffer.popleft()

    def as_text(self) -> str:
        lines = []
        for u, a in self.buffer:
            lines.append(f"用户: {u}")
            lines.append(f"助手: {a}")
        return "\n".join(lines)
