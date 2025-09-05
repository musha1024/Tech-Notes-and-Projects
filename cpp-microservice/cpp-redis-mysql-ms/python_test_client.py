#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import requests
import time

BASE = "http://127.0.0.1:8081"

def pretty(r):
    try:
        return r.status_code, r.json()
    except Exception:
        return r.status_code, r.text

def main():
    print("健康检查:")
    print(pretty(requests.get(BASE + "/health")))

    k = "demo_key"
    v = "hello_world"
    v2 = "hello_v2"

    print("\n1) 设置：write-through 写 MySQL + SETEX Redis")
    print(pretty(requests.post(BASE + "/set", json={"key": k, "value": v, "ttl": 10})))

    print("\n2) 读取：期望命中 Redis")
    print(pretty(requests.get(BASE + f"/get?key={k}")))

    print("\n3) 更新：write-through + 刷新缓存")
    print(pretty(requests.post(BASE + "/update", json={"key": k, "value": v2, "ttl": 15})))

    print("\n4) 读取：应命中 Redis 且为新值")
    print(pretty(requests.get(BASE + f"/get?key={k}")))

    print("\n5) 删除：删 MySQL + 失效 Redis")
    print(pretty(requests.delete(BASE + f"/del?key={k}")))

    print("\n6) 读取：应 404（如果 Redis 有旧值会被删除）")
    print(pretty(requests.get(BASE + f"/get?key={k}")))

    print("\n7) 读回源演示：先写入（短 TTL），等 TTL 过期再读")
    print(pretty(requests.post(BASE + "/set", json={"key": k, "value": "from_mysql", "ttl": 2})))
    time.sleep(3)  # 等 Redis 过期
    print("Redis 过期后读取 -> 触发 MySQL 回源并回填 Redis:")
    print(pretty(requests.get(BASE + f"/get?key={k}")))

def table_demo():
    T = "kv_tenant1"
    print("\n=== 多表演示：创建表并在该表读写 ===")
    print("创建表:", pretty(requests.post(BASE + "/table/create", json={"name": T})))
    print("SET (kv_tenant1):", pretty(requests.post(BASE + f"/set?table={T}", json={"key":"user:1","value":"alice","ttl":20})))
    print("GET (kv_tenant1):", pretty(requests.get(BASE + f"/get?table={T}&key=user:1")))
    print("DEL (kv_tenant1):", pretty(requests.delete(BASE + f"/del?table={T}&key=user:1")))
    print("GET after DEL:", pretty(requests.get(BASE + f"/get?table={T}&key=user:1")))

if __name__ == "__main__":
    main()
    table_demo()