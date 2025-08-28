#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include "rpc/protocol.h"

namespace rpc {

/**
 * RpcServer：注册方法（name->handler），接受连接并处理请求。
 * 教学实现：一连接一线程；异常转换为 status!=0 的响应。
 */
class RpcServer {
public:
    using Handler = std::function<Response(const Request&)>;

    explicit RpcServer(uint16_t port);
    ~RpcServer();

    void register_method(const std::string& name, Handler h);
    void serve(); // 阻塞监听（Ctrl+C 结束）

private:
    void handle_client(int cfd);

    uint16_t port_;
    int listen_fd_{-1};
    std::mutex mu_;
    std::map<std::string, Handler> handlers_;
};

} // namespace rpc
