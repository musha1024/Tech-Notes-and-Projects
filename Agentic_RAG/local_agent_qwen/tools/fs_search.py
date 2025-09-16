from pathlib import Path

def fs_search(root: str, keyword: str, max_results: int = 20) -> str:
    """
    在本地文件系统内递归搜索包含关键字的路径（大小写不敏感）。
    返回匹配路径（相对 root）的前若干条。
    """
    base = Path(root)
    if not base.exists():
        return f"[错误] 根目录不存在: {root}"
    kw = keyword.lower()
    out = []
    for p in base.rglob("*"):
        name = p.name.lower()
        if kw in name:
            out.append(str(p.relative_to(base)))
            if len(out) >= max_results:
                break
    if not out:
        return "(未找到匹配项)"
    return "\n".join(out)

def register(registry):
    registry.register(
        name="fs_search",
        func=fs_search,
        description="在给定根目录下按名称关键词搜索文件/文件夹",
        schema={
            "type": "object",
            "properties": {
                "root": {"type": "string"},
                "keyword": {"type": "string"},
                "max_results": {"type": "integer", "default": 20}
            },
            "required": ["root", "keyword"]
        }
    )
