#include "rpc/client.h"
#include "rpc/value.h"
#include <iostream>

using namespace rpc;

int main(int argc, char** argv){
    // ================= 程序功能说明 =================
    // 这是一个 RPC 客户端示例程序，负责：
    //  1. 从命令行参数中获取服务端的 host 和 port。
    //  2. 连接到 RPC 服务端。
    //  3. 发送两个调用请求：
    //     (1) 调用远程方法 "add"，传入参数 (7, 35)，打印加法结果。
    //     (2) 调用远程方法 "echo"，传入参数 "hello rpc"，打印回显结果。
    //  4. 打印服务端返回的结果或错误信息。
    //  5. 关闭连接并退出。
    //
    // ================ 输入 & 输出 ===================
    // 外部输入：
    //   - 命令行参数 argv[1]: 服务端主机地址（如 "127.0.0.1"）
    //   - 命令行参数 argv[2]: 服务端端口号（如 "8080"）
    //
    // 输出：
    //   - 如果调用成功，打印结果，例如：
    //       [client] add result = 42
    //       [client] echo result = hello rpc
    //   - 如果调用失败，打印错误状态码和错误消息，例如：
    //       [client] add error: (1) function not found

    if (argc < 3){
        std::cerr << "Usage: " << argv[0] << " <host> <port>\n";
        return 1;
    }
    std::string host = argv[1];
    uint16_t port = (uint16_t)std::stoi(argv[2]);

    // 创建 RPC 客户端对象，连接到指定的 host:port
    RpcClient c(host, port);
    c.connect_server();

    // 1) 调用远程 add(7, 35)
    {
        Response r = c.call("add", { Value::make_int(7), Value::make_int(35) });
        if (r.status == 0 && r.has_result && r.result.type == ValueType::INT64)
            std::cout << "[client] add result = " << r.result.i64 << "\n";
        else
            std::cout << "[client] add error: (" << r.status << ") " << r.err_msg << "\n";
    }

    // 2) 调用远程 echo("hello rpc")
    {
        Response r = c.call("echo", { Value::make_str("hello rpc") });
        if (r.status == 0 && r.has_result && r.result.type == ValueType::STRING)
            std::cout << "[client] echo result = " << r.result.str << "\n";
        else
            std::cout << "[client] echo error: (" << r.status << ") " << r.err_msg << "\n";
    }

    // 关闭客户端连接
    c.close_client();
    return 0;
}
