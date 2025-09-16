import math

SAFE_NAMES = {
    k: getattr(math, k) for k in dir(math) if not k.startswith("_")
}
SAFE_NAMES.update({
    "abs": abs, "min": min, "max": max, "round": round
})

def _safe_eval(expr: str):
    code = compile(expr, "<calc>", "eval")
    for name in code.co_names:
        if name not in SAFE_NAMES:
            raise NameError(f"非法名称: {name}")
    return eval(code, {"__builtins__": {}}, SAFE_NAMES)

def calculator(expression: str) -> str:
    """安全计算器，仅支持 math 中的函数与基本算术。"""
    value = _safe_eval(expression)
    return str(value)

def register(registry):
    registry.register(
        name="calculator",
        func=calculator,
        description="安全数学计算器（支持 + - * / ** sqrt sin cos 等）",
        schema={
            "type": "object",
            "properties": {
                "expression": {"type": "string", "description": "待计算的表达式，如 'sin(pi/6)+sqrt(2)'"}
            },
            "required": ["expression"]
        }
    )
