import re

_MD_IMAGE = re.compile(r"!\[[^\]]*\]\([^)]+\)")
_MD_LINK  = re.compile(r"\[[^\]]*\]\([^)]+\)")
_URL      = re.compile(r"https?://\S+")
_MULT     = re.compile(r"([!*#_\-\[\]\(\)~`|\\]{1,})")
_WS       = re.compile(r"[ \t]+")

def sanitize(text: str) -> str:
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    text = _MD_IMAGE.sub(" ", text)
    text = _MD_LINK.sub(" ", text)
    text = _URL.sub(" ", text)
    text = text.replace("```", " ").replace("`", " ")
    text = _MULT.sub(" ", text)
    text = _WS.sub(" ", text)
    text = re.sub(r"\n{3,}", "\n\n", text)
    return text.strip()

def normalize_text(t: str) -> str:
    t = sanitize(t)
    t = re.sub(r"[ \t]+", " ", t)
    t = re.sub(r"\n{3,}", "\n\n", t).strip()
    return t

def split_into_chunks(text: str, chunk_size: int = 900, overlap: int = 120):
    chunks, i, n = [], 0, len(text)
    while i < n:
        end = min(n, i + chunk_size)
        part = text[i:end].strip()
        if part: chunks.append(part)
        if end == n: break
        i = end - overlap if overlap > 0 else end
        if i < 0: i = end
    return chunks

def despace_cjk(s: str) -> str:
    cjk = r"\u4e00-\u9fff"
    s = re.sub(fr"(?<=[{cjk}])\s+(?=[{cjk}])", "", s)
    s = re.sub(fr"(?<=[{cjk}])\s+(?=[A-Za-z0-9])", "", s)
    s = re.sub(fr"(?<=[A-Za-z0-9])\s+(?=[{cjk}])", "", s)
    return s

def fold_acronym(s: str) -> str:
    # 把 R A G / L L M / Q W E N 这种单字母空格折叠成 RAG/LLM/QWEN
    return re.sub(r"\b([A-Za-z])(?:\s+([A-Za-z]))+\b",
                  lambda m: "".join(re.findall(r"[A-Za-z]", m.group(0))), s)

def post_clean(s: str) -> str:
    s = _MD_IMAGE.sub(" ", s)
    s = _MD_LINK.sub(" ", s)
    s = _URL.sub(" ", s)
    s = re.sub(r"data/raw/\S+", " ", s)
    s = re.sub(r"[!\[\]\(\)#]{1,}", " ", s)
    s = re.sub(r"\s{2,}", " ", s)
    s = s.strip()
    s = despace_cjk(s)
    s = fold_acronym(s)
    return s

def post_finalize(s: str) -> str:
    # 去掉首尾引号，遇到 stop 标记提前截断
    for stop in ("<|im_end|>", "</s>"):
        if stop in s:
            s = s.split(stop, 1)[0]
    s = s.strip().strip('"“”\'`').strip()
    return s
