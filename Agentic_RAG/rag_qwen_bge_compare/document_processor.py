#!/usr/bin/env python3
import os, argparse, json
from pathlib import Path
from tqdm import tqdm
from utils import normalize_text, split_into_chunks

TEXT_SUFFIX = (".txt", ".md", ".log", ".json")

def process_dir(in_dir: str, out_dir: str, chunk_size: int, overlap: int):
    in_dir, out_dir = Path(in_dir), Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    files = []
    for root, _, fs in os.walk(in_dir):
        for f in fs:
            if f.lower().endswith(TEXT_SUFFIX):
                files.append(Path(root) / f)
    out_jsonl = out_dir / "processed.jsonl"
    with open(out_jsonl, "w", encoding="utf-8") as w:
        for p in tqdm(files, desc="Processing"):
            try:
                raw = open(p, "r", encoding="utf-8", errors="ignore").read()
                norm = normalize_text(raw)
                chunks = split_into_chunks(norm, chunk_size, overlap)
                for idx, ch in enumerate(chunks):
                    rec = {"text": ch, "metadata": {"source": str(p), "chunk_id": idx}}
                    w.write(json.dumps(rec, ensure_ascii=False) + "\n")
            except Exception as e:
                print(f"[WARN] skip {p}: {e}")
    print(f"[OK] {out_jsonl}")

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--in_dir", required=True)
    ap.add_argument("--out_dir", required=True)
    ap.add_argument("--chunk_size", type=int, default=900)
    ap.add_argument("--overlap", type=int, default=120)
    a = ap.parse_args()
    process_dir(a.in_dir, a.out_dir, a.chunk_size, a.overlap)
