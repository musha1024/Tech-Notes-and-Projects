# C++ 多人聊天室

## 构建
```bash
mkdir build && cd build
cmake ..
cmake --build . -j
```

## 运行
### 启动服务端（默认 7777）
```bash
./chat_server 7777
```


### 启动多个客户端
在不同终端中运行：
```bash
./chat_client 127.0.0.1 7777 Alice
./chat_client 127.0.0.1 7777 Bob
./chat_client 127.0.0.1 7777 Carol
```