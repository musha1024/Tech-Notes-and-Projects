#include "rpc/server.h"
#include "rpc/value.h"
#include <iostream>

using namespace rpc;

// 示例方法：add(a:int64, b:int64) -> int64
static Response handle_add(const Request& req){
    Response rsp;
    try{
        int64_t a = as_i64(req.args, 0);
        int64_t b = as_i64(req.args, 1);
        rsp.status = 0;
        rsp.has_result = true;
        rsp.result = Value::make_int(a + b);
    }catch(const std::exception& e){
        rsp.status = 1;
        rsp.err_msg = e.what();
        rsp.has_result = false;
    }
    return rsp;
}

// 示例方法：echo(s:string) -> string
static Response handle_echo(const Request& req){
    Response rsp;
    try{
        std::string s = as_str(req.args, 0);
        rsp.status = 0;
        rsp.has_result = true;
        rsp.result = Value::make_str("echo: " + s);
    }catch(const std::exception& e){
        rsp.status = 1;
        rsp.err_msg = e.what();
        rsp.has_result = false;
    }
    return rsp;
}

int main(int argc, char** argv){
    if (argc < 2){
        std::cerr << "Usage: " << argv[0] << " <port>\n";
        return 1;
    }
    uint16_t port = (uint16_t)std::stoi(argv[1]);
    RpcServer s(port);
    s.register_method("add",  handle_add);
    s.register_method("echo", handle_echo);
    sserve:
    s.serve();
    return 0;
}
