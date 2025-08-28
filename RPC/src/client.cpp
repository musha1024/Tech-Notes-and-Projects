#include "rpc/client.h"
#include "rpc/frame.h"
#include "rpc/net.h"
#include <iostream>

namespace rpc {

RpcClient::RpcClient(std::string host, uint16_t port)
    : host_(std::move(host)), port_(port) {}

RpcClient::~RpcClient(){ close_client(); }

void RpcClient::connect_server(){
    fd_ = tcp_connect(host_, port_);
    std::cout << "[client] connected to " << host_ << ":" << port_ << "\n";
}

void RpcClient::close_client(){
    if (fd_ >= 0) close_fd(fd_);
    fd_ = -1;
}

Response RpcClient::call(const std::string& method, const std::vector<Value>& args){
    uint32_t id = next_id_++;
    Request req{ id, method, args };

    std::vector<uint8_t> frame;
    build_request_frame(req, frame);
    send_frame(fd_, frame);

    while (true){
        auto rf_opt = recv_frame(fd_);
        if (!rf_opt) throw std::runtime_error("server closed");
        auto rf = *rf_opt;
        if (rf.type != MsgType::RESPONSE) continue;
        if (rf.req_id != id) continue; // 简化处理：非当前请求的响应直接丢弃
        return parse_response_payload(rf.req_id, rf.payload);
    }
}

} // namespace rpc
