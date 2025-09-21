
from pathlib import Path
import base64

def write_local_file(path: str, content: str, as_text: bool = True, encoding: str = "utf-8", 
                     binary_is_base64: bool = True, exist_ok: bool = True, append: bool = False) -> str:
    """
    写入本地文件。
    - path: 目标路径；自动创建父目录
    - content: 写入内容；二进制模式下默认认为是base64字符串
    - as_text: True -> 文本写入；False -> 二进制写入
    - encoding: 文本编码
    - binary_is_base64: 二进制内容是否为base64字符串（True 时进行解码）
    - exist_ok: 目标存在是否允许覆盖（文本覆盖/二进制覆盖或追加取决于 append）
    - append: 以追加模式写入（与 as_text 联动：'a' 或 'ab'）
    返回：写入结果说明（路径/写入字节数）。
    """
    p = Path(path)
    try:
        p.parent.mkdir(parents=True, exist_ok=True)
        if p.exists() and not exist_ok and not append:
            return f"[错误] 目标已存在且 exist_ok=False: {path}"
        if as_text:
            mode = "a" if append else "w"
            data = content
            with p.open(mode, encoding=encoding) as f:
                f.write(data)
            return f"[写入成功][文本] 路径={p} 写入字符数={len(data)} append={append}"
        else:
            mode = "ab" if append else "wb"
            if binary_is_base64:
                try:
                    data = base64.b64decode(content)
                except Exception as e:
                    return f"[错误] base64 解码失败: {e}"
            else:
                # 直接把字符串按latin-1编码为字节（不推荐，除非明确知道是原始字节转成的str）
                data = content.encode('latin-1', errors='ignore')
            with p.open(mode) as f:
                f.write(data)
            return f"[写入成功][二进制] 路径={p} 写入字节数={len(data)} append={append}"
    except Exception as e:
        return f"[运行错误] {e}"


def register(registry):
    registry.register(
        name="write_local_file",
        func=write_local_file,
        description="写入本地文件（文本/二进制，支持base64、覆盖/追加）",
        schema={
            "type": "object",
            "properties": {
                "path": {"type": "string", "description": "要写入的文件路径"},
                "content": {"type": "string", "description": "文本或base64（二进制）内容"},
                "as_text": {"type": "boolean", "default": True},
                "encoding": {"type": "string", "default": "utf-8"},
                "binary_is_base64": {"type": "boolean", "default": True},
                "exist_ok": {"type": "boolean", "default": True},
                "append": {"type": "boolean", "default": False}
            },
            "required": ["path", "content"]
        }
    )
