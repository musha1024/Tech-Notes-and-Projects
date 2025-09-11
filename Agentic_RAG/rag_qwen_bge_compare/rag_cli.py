#!/usr/bin/env python3
import argparse
from typing import Dict, List
import yaml, torch, chromadb
from chromadb.config import Settings
from sentence_transformers import SentenceTransformer
from transformers import AutoTokenizer, AutoModelForCausalLM, pipeline
from reranker import Reranker
from utils import sanitize, post_clean, post_finalize


def _build_badwords(tok):
    ban_chars = ["!", "[", "]", "(", ")", "#", "！", "（", "）", "【", "】"]
    out=[]
    for ch in ban_chars:
        ids=tok.encode(ch, add_special_tokens=False)
        if ids: out.append(ids)
    for seq in ["![](", "[](", "](", "![", "http://", "https://", "data/raw/"]:
        ids=tok.encode(seq, add_special_tokens=False)
        if ids: out.append(ids)
    return out

def _cleanup_generation_config(model):
    gc = model.generation_config
    for k in ("temperature", "top_p", "top_k"):
        try: setattr(gc, k, None)
        except Exception: pass
    model.generation_config = gc

def load_cfg(p="config.yaml"): return yaml.safe_load(open(p, "r", encoding="utf-8"))
def load_embedder(path, device): return SentenceTransformer(path, device=device)

def retrieve(col, embedder, q, top_k=18):
    qv = embedder.encode([q], normalize_embeddings=True).tolist()[0]
    r = col.query(query_embeddings=[qv], n_results=top_k, include=["documents","metadatas","distances"])
    return [{"text":d,"metadata":m,"sim":1-dist,"rank":i} for i,(d,m,dist) in enumerate(zip(r["documents"][0], r["metadatas"][0], r["distances"][0]),1)]

def build_messages(q, ctxs):
    sys = "你是严谨的中文知识助手。只依据提供的资料回答；若资料不足，请输出：『根据提供的资料无法完全回答该问题。』不要输出链接、图片或 Markdown 语法。用简明中文作答，并避免使用括号、方括号、感叹号等符号。"
    blocks=[]
    for i,c in enumerate(ctxs,1):
        src=c["metadata"].get("source","未知来源")
        t=sanitize(c["text"]).replace("\n"," ").strip()
        if len(t)>600: t=t[:600]+"…"
        blocks.append(f"资料{i}：来源：{src}；内容：{t}")
    user="请基于下列资料回答问题：\n\n"+"\n\n".join(blocks)+f"\n\n问题：{q}\n回答："
    return [{"role":"system","content":sys},{"role":"user","content":user}]

def load_generator(path, gcfg):
    dev=gcfg.get("device","auto")
    li4=bool(gcfg.get("load_in_4bit",False))
    if dev=="auto": dev="cuda" if torch.cuda.is_available() else "cpu"
    tok=AutoTokenizer.from_pretrained(path, local_files_only=True, trust_remote_code=True)
    if tok.pad_token_id is None and tok.eos_token_id is not None: tok.pad_token_id=tok.eos_token_id
    kw={}
    if dev!="cpu":
        kw.update(dict(device_map="auto", torch_dtype=torch.float16))
        if li4:
            try:
                from transformers import BitsAndBytesConfig
                kw["quantization_config"]=BitsAndBytesConfig(load_in_4bit=True, bnb_4bit_quant_type="nf4", bnb_4bit_compute_dtype=torch.float16)
            except Exception as e:
                print(f"[WARN] bitsandbytes 不可用，回退到 fp16：{e}")
    else:
        kw.update(dict(torch_dtype=torch.float32))
    model=AutoModelForCausalLM.from_pretrained(path, local_files_only=True, trust_remote_code=True, **kw)
    _cleanup_generation_config(model)
    gen=pipeline("text-generation", model=model, tokenizer=tok, return_full_text=False)
    bad_ids=_build_badwords(tok)
    def generate(messages, **gen_kwargs):
        prompt=tok.apply_chat_template(messages, tokenize=False, add_generation_prompt=True)
        return gen(prompt, **gen_kwargs)[0]["generated_text"]
    return tok, generate, bad_ids

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--chroma_dir", required=True)
    ap.add_argument("--collection", default="default")
    ap.add_argument("--config", default="config.yaml")
    a=ap.parse_args()

    cfg=load_cfg(a.config)
    top_k=int(cfg.get("top_k",18)); top_r=int(cfg.get("top_rerank",4))
    embedder=load_embedder(cfg["embedding_model_path"], cfg.get("embedding_device","cpu"))
    client=chromadb.PersistentClient(path=a.chroma_dir, settings=Settings(anonymized_telemetry=False))
    col=client.get_collection(a.collection)
    reranker=Reranker(cfg["reranker_model_path"], device=cfg.get("reranker_device","cpu"))
    gen_cfg=cfg.get("generator",{})
    tok, generate, bad_words_ids=load_generator(cfg["generator_model_path"], gen_cfg)

    print("==== RAG (bge-m3 + reranker + Qwen2.5-7B) ====")
    print(f"[Embed] {cfg['embedding_model_path']} on {cfg.get('embedding_device','cpu')}")
    print(f"[Rerank] {cfg['reranker_model_path']} on {cfg.get('reranker_device','cpu')}")
    print(f"[Gen  ] {cfg['generator_model_path']} (4bit={bool(gen_cfg.get('load_in_4bit', False))})")
    print("输入你的问题（'exit' 退出）：")

    while True:
        try:
            q=input("\n❓ 问题: ").strip()
            q=sanitize(q)
            if q.lower() in ("exit","quit","q"): print("👋 再见"); break
            if len(q)<2: print("请输入有效问题"); continue
            items=retrieve(col, embedder, q, top_k=top_k)
            if not items: print("未检索到相关资料"); continue
            items=reranker.rerank(q, items, top_r=top_r)
            messages=build_messages(q, items)
            gen_args={
                "max_new_tokens": int(gen_cfg.get("max_new_tokens",512)),
                "min_new_tokens": 64,
                "repetition_penalty": float(gen_cfg.get("repetition_penalty",1.1)),
                "eos_token_id": tok.eos_token_id, "pad_token_id": tok.pad_token_id,
                "bad_words_ids": bad_words_ids,
                "do_sample": False, "num_beams": 1,
            }
            out=generate(messages, **gen_args).strip()
            print("\n✨ 回答：\n", post_finalize(post_clean(out)))
            print("\n📖 参考来源（重排后）：")
            for c in items:
                print(f"  - {c['metadata'].get('source','未知')} (rerank={c.get('rerank_score',0):.3f})")
        except KeyboardInterrupt:
            print("\n中断，退出"); break
        except Exception as e:
            print(f"[ERROR] {e}")

if __name__=="__main__": main()
