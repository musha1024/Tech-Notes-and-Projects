# Redis Microservice Test Report (20250904-081918)

- Base URL: `http://127.0.0.1:8080`
- Total: 10, Pass: 10, Fail: 0, Pass rate: 100.0%

| Case | Pass | Status | Latency(ms) | Note |
|---|:---:|---:|---:|---|
| ping | ✅ | 200 | 2 | health check |
| set text | ✅ | 200 | 1 | SETEX 60s |
| get text | ✅ | 200 | 0 | GET should return Alice |
| exists true | ✅ | 200 | 0 | EXISTS(user:1)=true |
| incr +2 | ✅ | 200 | 1 | INCRBY 2 |
| incr +1 | ✅ | 200 | 0 | INCR default |
| get missing | ✅ | 200 | 0 | GET null |
| del text | ✅ | 200 | 0 | DEL user:1 |
| del again | ✅ | 200 | 0 | DEL again -> 0 |
| invalid key | ✅ | 400 | 0 | Key fails whitelist -> 400 |

## Details

### ping

**Request**
```json
{
  "method": "GET",
  "url": "http://127.0.0.1:8080/ping"
}
```
**Response**
```json
{
  "ok": true,
  "status": 200,
  "headers": {
    "Content-Type": "application/json; charset=utf-8",
    "Content-Length": "11",
    "Connection": "close"
  },
  "body": "{\"ok\":true}",
  "elapsed_ms": 2
}
```

### set text

**Request**
```json
{
  "method": "POST",
  "url": "http://127.0.0.1:8080/set",
  "json": {
    "key": "user:1",
    "value": "Alice",
    "ttl": 60
  }
}
```
**Response**
```json
{
  "ok": true,
  "status": 200,
  "headers": {
    "Content-Type": "application/json; charset=utf-8",
    "Content-Length": "11",
    "Connection": "close"
  },
  "body": "{\"ok\":true}",
  "elapsed_ms": 1
}
```

### get text

**Request**
```json
{
  "method": "GET",
  "url": "http://127.0.0.1:8080/get?key=user%3A1"
}
```
**Response**
```json
{
  "ok": true,
  "status": 200,
  "headers": {
    "Content-Type": "application/json; charset=utf-8",
    "Content-Length": "32",
    "Connection": "close"
  },
  "body": "{\"key\":\"user:1\",\"value\":\"Alice\"}",
  "elapsed_ms": 0
}
```

### exists true

**Request**
```json
{
  "method": "GET",
  "url": "http://127.0.0.1:8080/exists?key=user%3A1"
}
```
**Response**
```json
{
  "ok": true,
  "status": 200,
  "headers": {
    "Content-Type": "application/json; charset=utf-8",
    "Content-Length": "15",
    "Connection": "close"
  },
  "body": "{\"exists\":true}",
  "elapsed_ms": 0
}
```

### incr +2

**Request**
```json
{
  "method": "POST",
  "url": "http://127.0.0.1:8080/incr",
  "json": {
    "key": "cnt:unit",
    "by": 2
  }
}
```
**Response**
```json
{
  "ok": true,
  "status": 200,
  "headers": {
    "Content-Type": "application/json; charset=utf-8",
    "Content-Length": "9",
    "Connection": "close"
  },
  "body": "{\"new\":5}",
  "elapsed_ms": 1
}
```

### incr +1

**Request**
```json
{
  "method": "POST",
  "url": "http://127.0.0.1:8080/incr",
  "json": {
    "key": "cnt:unit"
  }
}
```
**Response**
```json
{
  "ok": true,
  "status": 200,
  "headers": {
    "Content-Type": "application/json; charset=utf-8",
    "Content-Length": "9",
    "Connection": "close"
  },
  "body": "{\"new\":6}",
  "elapsed_ms": 0
}
```

### get missing

**Request**
```json
{
  "method": "GET",
  "url": "http://127.0.0.1:8080/get?key=no%3Asuch%3Akey"
}
```
**Response**
```json
{
  "ok": true,
  "status": 200,
  "headers": {
    "Content-Type": "application/json; charset=utf-8",
    "Content-Length": "34",
    "Connection": "close"
  },
  "body": "{\"key\":\"no:such:key\",\"value\":null}",
  "elapsed_ms": 0
}
```

### del text

**Request**
```json
{
  "method": "DELETE",
  "url": "http://127.0.0.1:8080/del?key=user%3A1"
}
```
**Response**
```json
{
  "ok": true,
  "status": 200,
  "headers": {
    "Content-Type": "application/json; charset=utf-8",
    "Content-Length": "13",
    "Connection": "close"
  },
  "body": "{\"deleted\":1}",
  "elapsed_ms": 0
}
```

### del again

**Request**
```json
{
  "method": "DELETE",
  "url": "http://127.0.0.1:8080/del?key=user%3A1"
}
```
**Response**
```json
{
  "ok": true,
  "status": 200,
  "headers": {
    "Content-Type": "application/json; charset=utf-8",
    "Content-Length": "13",
    "Connection": "close"
  },
  "body": "{\"deleted\":0}",
  "elapsed_ms": 0
}
```

### invalid key

**Request**
```json
{
  "method": "GET",
  "url": "http://127.0.0.1:8080/get?key=bad%20key%20with%20space"
}
```
**Response**
```json
{
  "ok": false,
  "status": 400,
  "headers": {
    "Content-Type": "application/json; charset=utf-8",
    "Content-Length": "34",
    "Connection": "close"
  },
  "body": "{\"error\":\"invalid or missing key\"}",
  "elapsed_ms": 0
}
```
