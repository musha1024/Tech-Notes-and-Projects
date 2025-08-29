#include "rpc/server.h"
#include "rpc/net.h"
#include <iostream>

namespace rpc {

// =====================================================
// RpcServer: 简易 RPC 服务端
// 职责：
//   - 监听指定端口，接受客户端 TCP 连接
//   - 解析收到的请求帧 → 调用已注册的方法 → 回包
//   - 每个客户端连接由独立线程处理（演示用）
//
// 输入来源：客户端发来的二进制帧（frame）
// 输出对象：返回的二进制帧（response frame）写回到 TCP 连接
// =====================================================

RpcServer::RpcServer(uint16_t port) : port_(port) {}

// 析构：关闭监听 fd（若已打开）
RpcServer::~RpcServer(){ if (listen_fd_>=0) close_fd(listen_fd_); }

// =====================================================
// register_method(name, handler)
// 功能：注册一个 RPC 方法及其处理函数
// 输入：name 方法名；handler 签名为 Response(const Request&)
// 输出：无；内部会覆盖同名方法
// 线程安全：用互斥锁保护 handlers_
// =====================================================
void RpcServer::register_method(const std::string& name, Handler h){
    std::lock_guard<std::mutex> lk(mu_);
    handlers_[name] = std::move(h);
}

// =====================================================
// serve()
// 功能：启动服务端主循环
// 步骤：tcp_listen -> 打印监听日志 -> accept 循环
//       每来一个连接就开线程 handle_client
//
// 输入：无（使用构造传入的 port_）
// 输出：无（阻塞运行；如需退出可在外层中断进程或扩展退出条件）
// 失败：底层 socket 出错会 die() 退出；accept 失败则打日志继续
// =====================================================
void RpcServer::serve(){
    listen_fd_ = tcp_listen(port_);
    std::cout << "[server] listening on 0.0.0.0:" << port_ << "\n";
    while (true){
        sockaddr_in cli{}; socklen_t len = sizeof(cli);
        int cfd = ::accept(listen_fd_, (sockaddr*)&cli, &len);
        if (cfd < 0) { perror("accept"); continue; }
        // 生产环境建议使用线程池或事件驱动；这里演示用每连接一线程
        std::thread(&RpcServer::handle_client, this, cfd).detach();
    }
}

// =====================================================
// handle_client(cfd)
// 功能：单连接读写循环
// 流程：
//   1) 读取一帧 -> 解析为 RawFrame
//   2) 若是 REQUEST：解析 payload → 定位 handler → 执行 → 回包
//   3) 非 REQUEST 或解析异常：返回错误 Response
//   4) 对端关闭：结束循环，关闭 cfd
//
// 输入：cfd 客户端连接的 fd（演示用 int，跨平台建议 socket_t）
// 输出：无（通过 send_frame 写回响应）
// 返回：void
// 边界/异常：
//   - recv_frame 返回 nullopt 视为对端关闭
//   - 任意解析/业务异常都会被 catch，封装成 status=2 的错误响应
// =====================================================
void RpcServer::handle_client(int cfd){
    std::cout << "[server] new client fd=" << cfd << "\n";
    while (true){
        // 读取一帧（含长度前缀），转为 RawFrame
        auto rf_opt = recv_frame(cfd);
        if (!rf_opt){
            std::cout << "[server] client closed fd=" << cfd << "\n";
            break;
        }
        auto rf = *rf_opt;

        try{
            if (rf.type == MsgType::REQUEST){
                // 1) 解析请求 payload → Request
                Request req = parse_request_payload(rf.req_id, rf.method, rf.payload);

                // 2) 查找处理函数（加锁保护 handlers_）
                Handler h;
                {
                    std::lock_guard<std::mutex> lk(mu_);
                    auto it = handlers_.find(req.method);
                    if (it != handlers_.end()) h = it->second;
                }

                // 3) 调用或返回“未知方法”
                Response rsp;
                if (!h){
                    rsp.req_id = req.req_id;
                    rsp.status = 1;
                    rsp.err_msg = "unknown method: " + req.method;
                    rsp.has_result = false;
                }else{
                    rsp = h(req);            // 业务代码可能抛异常 → 外层 catch
                    rsp.req_id = req.req_id; // 保证回包 req_id 对齐
                }

                // 4) 编码为 response frame 并发送
                std::vector<uint8_t> frame;
                build_response_frame(rsp, frame);
                send_frame(cfd, frame);
            }else{
                // 收到非响应类型帧（比如客户端实现错误）
                std::cerr << "[server] unexpected frame type\n";
            }
        }catch(const std::exception& e){
            // 将异常封装为错误响应（status=2）
            Response rsp;
            rsp.req_id = rf.req_id;
            rsp.status = 2;
            rsp.err_msg = std::string("server exception: ") + e.what();
            rsp.has_result = false;

            std::vector<uint8_t> frame;
            build_response_frame(rsp, frame);
            send_frame(cfd, frame);
        }
    }
    // 连接退出：关闭 fd
    close_fd(cfd);
}

} // namespace rpc
