#!/usr/bin/env python3
import argparse, json
from pathlib import Path
import chromadb
from chromadb.config import Settings
from sentence_transformers import SentenceTransformer
import yaml

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--processed_dir", required=True)
    ap.add_argument("--chroma_dir", required=True)
    ap.add_argument("--collection", default="default")
    args = ap.parse_args()

    cfg = yaml.safe_load(open("config.yaml", "r", encoding="utf-8"))
    model = SentenceTransformer(cfg["embedding_model_path"], device=cfg.get("embedding_device","cpu"))
    client = chromadb.PersistentClient(path=args.chroma_dir, settings=Settings(anonymized_telemetry=False))
    try: col = client.get_collection(args.collection)
    except: col = client.create_collection(args.collection)

    jsonl = Path(args.processed_dir) / "processed.jsonl"
    ids, docs, metas = [], [], []
    for i, line in enumerate(open(jsonl, "r", encoding="utf-8")):
        rec = json.loads(line)
        ids.append(f"doc_{i}"); docs.append(rec["text"]); metas.append(rec["metadata"])

    embs = model.encode(docs, normalize_embeddings=True, show_progress_bar=True).tolist()
    try: col.delete(ids=ids)
    except: pass
    col.add(ids=ids, documents=docs, metadatas=metas, embeddings=embs)
    print(f"[OK] 写入集合 {args.collection}，共 {len(ids)} 条")
    print(f"[OK] Chroma at {args.chroma_dir}")

if __name__ == "__main__":
    main()
