#pragma once
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>
#include "rpc/protocol.h"

namespace rpc {

/**
 * RpcClient：同步调用示例。简化为“发一个等一个”，便于理解。
 * 后续可以增加接收线程 + promise/future 做并发路由。
 */
class RpcClient {
public:
    RpcClient(std::string host, uint16_t port);
    ~RpcClient();

    void connect_server();
    void close_client();

    Response call(const std::string& method, const std::vector<Value>& args);

private:
    std::string host_;
    uint16_t port_;
    int fd_{-1};
    std::atomic<uint32_t> next_id_{1};
};

} // namespace rpc
