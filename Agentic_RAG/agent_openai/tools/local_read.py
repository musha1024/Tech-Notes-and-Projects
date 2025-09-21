
from pathlib import Path

def read_local_file(path: str, as_text: bool = True, encoding: str = "utf-8", 
                    start: int = 0, max_bytes: int = 1_000_000) -> str:
    """
    读取本地文件（默认按文本）。
    - path: 文件路径
    - as_text: True -> 以文本方式读取；False -> 以二进制读取并返回十六进制前缀
    - encoding: 文本编码
    - start: 从第几个字节偏移开始读取（支持大文件分页）
    - max_bytes: 最大读取字节数（默认 1MB），避免 Observation 过长
    返回：简短的字符串（包含长度与片段信息）。
    """
    p = Path(path)
    if not p.exists():
        return f"[错误] 文件不存在: {path}"
    if not p.is_file():
        return f"[错误] 不是普通文件: {path}"

    try:
        if as_text:
            # 读取指定字节范围再按编码解码，避免一次性读取超大文件
            with p.open("rb") as f:
                f.seek(max(0, int(start)))
                data = f.read(max(0, int(max_bytes)))
            try:
                text = data.decode(encoding, errors="replace")
            except LookupError:
                return f"[错误] 不支持的编码: {encoding}"
            return (
                f"[读取成功][文本] 路径={p} 偏移={start} 字节数={len(data)}\n"
                f"-------- 内容片段（最多 {max_bytes} 字节）--------\n"
                f"{text}"
            )
        else:
            with p.open("rb") as f:
                f.seek(max(0, int(start)))
                data = f.read(max(0, int(max_bytes)))
            # 返回十六进制前缀，避免 Observation 里出现不可见字符
            hex_prefix = data.hex()
            if len(hex_prefix) > 1024:
                hex_prefix = hex_prefix[:1024] + "..."
            return (
                f"[读取成功][二进制] 路径={p} 偏移={start} 字节数={len(data)}\n"
                f"hex前缀(截断至1024字符): {hex_prefix}"
            )
    except Exception as e:
        return f"[运行错误] {e}"


def register(registry):
    registry.register(
        name="read_local_file",
        func=read_local_file,
        description="读取本地文件（支持文本/二进制，支持偏移+限长分页读取）",
        schema={
            "type": "object",
            "properties": {
                "path": {"type": "string", "description": "要读取的文件路径"},
                "as_text": {"type": "boolean", "default": True},
                "encoding": {"type": "string", "default": "utf-8"},
                "start": {"type": "integer", "default": 0},
                "max_bytes": {"type": "integer", "default": 1000000}
            },
            "required": ["path"]
        }
    )
