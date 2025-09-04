#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Redis Microservice End-to-End Tester (std-lib only)
Fixes:
- Use real HTTP DELETE for /del
- Capture HTTPError status/body so negative cases can PASS
"""

import json
import sys
import time
from urllib import request, parse, error
from pathlib import Path
from datetime import datetime

DEFAULT_BASE = "http://127.0.0.1:8080"

def http_request(method, url, json_payload=None, timeout=5):
    data = None
    headers = {}
    if json_payload is not None:
        data = json.dumps(json_payload).encode("utf-8")
        headers["Content-Type"] = "application/json"
    req = request.Request(url, data=data, method=method, headers=headers)
    start = time.time()
    try:
        with request.urlopen(req, timeout=timeout) as resp:
            body = resp.read().decode("utf-8", errors="replace")
            status = resp.status
            hdrs = dict(resp.getheaders())
            ok = True
    except error.HTTPError as he:  # 4xx/5xx still return a body
        body = he.read().decode("utf-8", errors="replace")
        status = he.code
        hdrs = dict(he.headers.items())
        ok = False
    except Exception as e:
        return {"ok": False, "error": str(e), "elapsed_ms": int((time.time()-start)*1000)}
    return {"ok": ok, "status": status, "headers": hdrs, "body": body, "elapsed_ms": int((time.time()-start)*1000)}

def run_suite(base=DEFAULT_BASE, results_dir=None):
    base = base.rstrip("/")
    ts = datetime.now().strftime("%Y%m%d-%H%M%S")
    outdir = Path(results_dir or f"./results/{ts}")
    outdir.mkdir(parents=True, exist_ok=True)

    cases = []
    def add_case(name, req, res, expect_ok=True, note=""):
        ok = False
        if "status" in res:
            if expect_ok:
                ok = 200 <= res["status"] < 300
            else:
                ok = res["status"] >= 400
        case = {
            "name": name,
            "request": req,
            "response": res,
            "pass": bool(ok),
            "note": note,
        }
        cases.append(case)
        print(f"[{'PASS' if ok else 'FAIL'}] {name}  ({res.get('elapsed_ms','?')} ms)")

    # 1) ping
    add_case(
        "ping",
        {"method":"GET","url":f"{base}/ping"},
        http_request("GET", f"{base}/ping"),
        expect_ok=True, note="health check"
    )

    k_text = "user:1"
    v_text = "Alice"
    k_counter = "cnt:unit"
    k_missing = "no:such:key"

    # 2) set with ttl
    add_case(
        "set text",
        {"method":"POST","url":f"{base}/set","json":{"key":k_text,"value":v_text,"ttl":60}},
        http_request("POST", f"{base}/set", {"key":k_text,"value":v_text,"ttl":60}),
        expect_ok=True, note="SETEX 60s"
    )

    # 3) get text
    add_case(
        "get text",
        {"method":"GET","url":f"{base}/get?key={parse.quote(k_text)}"},
        http_request("GET", f"{base}/get?key={parse.quote(k_text)}"),
        expect_ok=True, note="GET should return Alice"
    )

    # 4) exists (true)
    add_case(
        "exists true",
        {"method":"GET","url":f"{base}/exists?key={parse.quote(k_text)}"},
        http_request("GET", f"{base}/exists?key={parse.quote(k_text)}"),
        expect_ok=True, note="EXISTS(user:1)=true"
    )

    # 5) incr +2
    add_case(
        "incr +2",
        {"method":"POST","url":f"{base}/incr","json":{"key":k_counter,"by":2}},
        http_request("POST", f"{base}/incr", {"key":k_counter,"by":2}),
        expect_ok=True, note="INCRBY 2"
    )

    # 6) incr default +1
    add_case(
        "incr +1",
        {"method":"POST","url":f"{base}/incr","json":{"key":k_counter}},
        http_request("POST", f"{base}/incr", {"key":k_counter}),
        expect_ok=True, note="INCR default"
    )

    # 7) get missing => value:null
    add_case(
        "get missing",
        {"method":"GET","url":f"{base}/get?key={parse.quote(k_missing)}"},
        http_request("GET", f"{base}/get?key={parse.quote(k_missing)}"),
        expect_ok=True, note="GET null"
    )

    # 8) delete existing  (DELETE method)
    add_case(
        "del text",
        {"method":"DELETE","url":f"{base}/del?key={parse.quote(k_text)}"},
        http_request("DELETE", f"{base}/del?key={parse.quote(k_text)}"),
        expect_ok=True, note="DEL user:1"
    )

    # 9) delete again (0 deleted)
    add_case(
        "del again",
        {"method":"DELETE","url":f"{base}/del?key={parse.quote(k_text)}"},
        http_request("DELETE", f"{base}/del?key={parse.quote(k_text)}"),
        expect_ok=True, note="DEL again -> 0"
    )

    # 10) invalid key (should 400)
    bad_key = "bad key with space"
    add_case(
        "invalid key",
        {"method":"GET","url":f"{base}/get?key={parse.quote(bad_key)}"},
        http_request("GET", f"{base}/get?key={parse.quote(bad_key)}"),
        expect_ok=False, note="Key fails whitelist -> 400"
    )

    # Save artifacts
    results = {
        "base": base,
        "timestamp": ts,
        "cases": cases,
        "pass_rate": sum(1 for c in cases if c["pass"]) / max(1, len(cases)),
    }
    (outdir / "results.json").write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding="utf-8")

    lines = []
    lines.append(f"# Redis Microservice Test Report ({ts})\n")
    lines.append(f"- Base URL: `{base}`")
    lines.append(f"- Total: {len(cases)}, Pass: {sum(1 for c in cases if c['pass'])}, "
                 f"Fail: {sum(1 for c in cases if not c['pass'])}, "
                 f"Pass rate: {results['pass_rate']*100:.1f}%\n")
    lines.append("| Case | Pass | Status | Latency(ms) | Note |")
    lines.append("|---|:---:|---:|---:|---|")
    for c in cases:
        status = c["response"].get("status","-")
        lines.append(f"| {c['name']} | {'✅' if c['pass'] else '❌'} | {status} | {c['response'].get('elapsed_ms','?')} | {c['note']} |")
    lines.append("\n## Details\n")
    for c in cases:
        lines.append(f"### {c['name']}\n")
        lines.append("**Request**")
        lines.append("```json")
        lines.append(json.dumps(c["request"], ensure_ascii=False, indent=2))
        lines.append("```\n**Response**")
        lines.append("```json")
        lines.append(json.dumps(c["response"], ensure_ascii=False, indent=2))
        lines.append("```\n")
    (outdir / "report.md").write_text("\n".join(lines), encoding="utf-8")

    print("\n=== SUMMARY ===")
    print(f"Report dir: {outdir.resolve()}")
    print(f"Pass rate: {results['pass_rate']*100:.1f}%")
    # consider the whole suite a success if all cases pass (including negative one)
    return 0 if all(c["pass"] for c in cases) else 1

if __name__ == "__main__":
    base = DEFAULT_BASE
    out = None
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] == "--base" and i+1 < len(args):
            base = args[i+1]; i += 2
        elif args[i] == "--out" and i+1 < len(args):
            out = args[i+1]; i += 2
        else:
            print("Usage: python redis_microservice_test.py [--base http://127.0.0.1:8080] [--out results_dir]")
            sys.exit(2)
    sys.exit(run_suite(base, out))
