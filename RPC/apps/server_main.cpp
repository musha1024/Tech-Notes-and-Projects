#include "rpc/server.h"
#include "rpc/value.h"
#include <iostream>

using namespace rpc;

// ======================= 程序功能说明 =======================
// 这是一个 RPC 服务端示例程序，负责：
//   1. 监听指定端口，接受客户端的 TCP 连接。
//   2. 注册两个 RPC 方法：
//        - "add": 接收两个 int64 参数，返回它们的和。
//        - "echo": 接收一个字符串参数，返回 "echo: <参数>"。
//   3. 在主循环中持续处理来自客户端的请求，并将结果或错误返回。
// ============================================================

// 示例方法：add(a:int64, b:int64) -> int64
static Response handle_add(const Request& req){
    Response rsp;
    try{
        // 从请求参数中提取两个整数
        int64_t a = as_i64(req.args, 0);
        int64_t b = as_i64(req.args, 1);

        // 正常返回结果
        rsp.status = 0;
        rsp.has_result = true;
        rsp.result = Value::make_int(a + b);
    }catch(const std::exception& e){
        // 如果参数解析失败，返回错误状态和错误信息
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
    // ================ 输入 & 输出说明 =================
    // 外部输入：
    //   - 命令行参数 argv[1]: 监听的端口号（如 "8080"）
    //
    // 对外输出：
    //   - 服务端会在标准输出打印启动日志（如果你加了 log）。
    //   - 对客户端的 RPC 请求返回对应的结果，例如：
    //       "add" -> 返回整数和
    //       "echo" -> 返回带 "echo:" 前缀的字符串
    // ==================================================

    if (argc < 2){
        std::cerr << "Usage: " << argv[0] << " <port>\n";
        return 1;
    }
    uint16_t port = (uint16_t)std::stoi(argv[1]);

    // 创建 RPC 服务端并监听指定端口
    RpcServer s(port);

    // 注册方法
    s.register_method("add",  handle_add);
    s.register_method("echo", handle_echo);

    // 启动事件循环，阻塞等待并处理客户端请求
    s.serve();
    return 0;
}
