#include "rpc/client.h"
#include "rpc/frame.h"
#include "rpc/net.h"
#include <iostream>

namespace rpc {

// =======================================================
// RpcClient 类：封装 RPC 客户端逻辑
//   - 管理与服务端的 TCP 连接
//   - 发送请求（Request）并接收响应（Response）
//   - 对外提供 call(method, args) 接口，像本地函数一样调用远程方法
// =======================================================

// 构造函数：保存 host 和 port 信息
RpcClient::RpcClient(std::string host, uint16_t port)
    : host_(std::move(host)), port_(port) {}

// 析构函数：保证退出时关闭连接
RpcClient::~RpcClient(){ close_client(); }

// 建立到服务端的 TCP 连接
void RpcClient::connect_server(){
    fd_ = tcp_connect(host_, port_);
    std::cout << "[client] connected to " << host_ << ":" << port_ << "\n";
}

// 主动关闭客户端连接
void RpcClient::close_client(){
    if (fd_ >= 0) close_fd(fd_);
    fd_ = -1;
}

// =======================================================
// call(method, args):
//   - 构造一个请求 Request {id, method, args}
//   - 将请求序列化为字节帧并通过 TCP 发送
//   - 阻塞等待服务端返回 Response 帧
//   - 如果响应的 req_id 与当前请求的 id 匹配，则解析并返回 Response
//
// 输入：
//   - method: 远程方法名（如 "add"、"echo"）
//   - args  : 调用参数（Value 向量，支持 int、string 等）
//
// 输出：
//   - Response 对象（包含 status、错误信息、返回值）
//   - 如果服务端关闭连接，则抛出 runtime_error 异常
// =======================================================
Response RpcClient::call(const std::string& method, const std::vector<Value>& args){
    // 为请求分配一个唯一 id
    uint32_t id = next_id_++;
    Request req{ id, method, args };

    // 序列化请求帧并发送
    std::vector<uint8_t> frame;
    build_request_frame(req, frame);
    send_frame(fd_, frame);

    // 循环等待响应帧
    while (true){
        auto rf_opt = recv_frame(fd_);
        if (!rf_opt) throw std::runtime_error("server closed");

        auto rf = *rf_opt;
        if (rf.type != MsgType::RESPONSE) continue;  // 只关心响应消息
        if (rf.req_id != id) continue;               // 如果不是当前请求的响应，丢弃

        // 解析 payload，构造 Response 返回
        return parse_response_payload(rf.req_id, rf.payload);
    }
}

} // namespace rpc
