from dataclasses import dataclass, field
from typing import List, Dict, Any

@dataclass
class Message:
    role: str  # "system" | "user" | "assistant"
    content: str

@dataclass
class ToolSpec:
    name: str
    func: callable
    description: str
    schema: Dict[str, Any]  # JSON schema for Args

@dataclass
class ToolRegistry:
    tools: Dict[str, ToolSpec] = field(default_factory=dict)

    def register(self, name: str, func, description: str, schema: dict):
        self.tools[name] = ToolSpec(name, func, description, schema)

    def names(self):
        return list(self.tools.keys())

    def get(self, name: str) -> ToolSpec:
        return self.tools[name]
