#include "rpc/server.h"
#include "rpc/net.h"
#include <iostream>

namespace rpc {

RpcServer::RpcServer(uint16_t port) : port_(port) {}
RpcServer::~RpcServer(){ if (listen_fd_>=0) close_fd(listen_fd_); }

void RpcServer::register_method(const std::string& name, Handler h){
    std::lock_guard<std::mutex> lk(mu_);
    handlers_[name] = std::move(h);
}

void RpcServer::serve(){
    listen_fd_ = tcp_listen(port_);
    std::cout << "[server] listening on 0.0.0.0:" << port_ << "\n";
    while (true){
        sockaddr_in cli{}; socklen_t len = sizeof(cli);
        int cfd = ::accept(listen_fd_, (sockaddr*)&cli, &len);
        if (cfd < 0) { perror("accept"); continue; }
        std::thread(&RpcServer::handle_client, this, cfd).detach();
    }
}

void RpcServer::handle_client(int cfd){
    std::cout << "[server] new client fd=" << cfd << "\n";
    while (true){
        auto rf_opt = recv_frame(cfd);
        if (!rf_opt){
            std::cout << "[server] client closed fd=" << cfd << "\n";
            break;
        }
        auto rf = *rf_opt;
        try{
            if (rf.type == MsgType::REQUEST){
                Request req = parse_request_payload(rf.req_id, rf.method, rf.payload);

                Handler h;
                {
                    std::lock_guard<std::mutex> lk(mu_);
                    auto it = handlers_.find(req.method);
                    if (it != handlers_.end()) h = it->second;
                }

                Response rsp;
                if (!h){
                    rsp.req_id = req.req_id;
                    rsp.status = 1;
                    rsp.err_msg = "unknown method: " + req.method;
                    rsp.has_result = false;
                }else{
                    rsp = h(req);
                    rsp.req_id = req.req_id;
                }

                std::vector<uint8_t> frame;
                build_response_frame(rsp, frame);
                send_frame(cfd, frame);
            }else{
                std::cerr << "[server] unexpected frame type\n";
            }
        }catch(const std::exception& e){
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
    close_fd(cfd);
}

} // namespace rpc
