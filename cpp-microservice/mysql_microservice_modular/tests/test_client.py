#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import argparse, json, requests

def pretty(r):
    try: return json.dumps(r.json(), ensure_ascii=False, indent=2)
    except Exception: return f"status={r.status_code}, text={r.text[:200]}..."

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", default=8081, type=int)
    args = ap.parse_args()
    base = f"http://{args.host}:{args.port}"

    print("1) init")
    r = requests.post(f"{base}/admin/init"); print(pretty(r)); r.raise_for_status()
    print("2) create A")
    r = requests.post(f"{base}/users", json={"name":"Alice","email":"alice@example.com"}); print(pretty(r)); r.raise_for_status(); u1=r.json().get("id")
    print("3) create B")
    r = requests.post(f"{base}/users", json={"name":"Bob","email":"bob@example.com"}); print(pretty(r)); r.raise_for_status(); u2=r.json().get("id")
    print("4) list")
    r = requests.get(f"{base}/users?limit=10&offset=0"); print(pretty(r)); r.raise_for_status()
    print("5) get A")
    r = requests.get(f"{base}/users/{u1}"); print(pretty(r)); r.raise_for_status()
    print("6) update B")
    r = requests.put(f"{base}/users/{u2}", json={"name":"Bobby","email":"bobby@example.com"}); print(pretty(r)); r.raise_for_status()
    print("7) delete A")
    r = requests.delete(f"{base}/users/{u1}"); print(pretty(r)); r.raise_for_status()
    print("8) list again")
    r = requests.get(f"{base}/users?limit=10&offset=0"); print(pretty(r)); r.raise_for_status()
    print("✅ done.")

if __name__ == "__main__":
    main()
