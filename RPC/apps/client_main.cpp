#include "rpc/client.h"
#include "rpc/value.h"
#include <iostream>

using namespace rpc;

int main(int argc, char** argv){
    if (argc < 3){
        std::cerr << "Usage: " << argv[0] << " <host> <port>\n";
        return 1;
    }
    std::string host = argv[1];
    uint16_t port = (uint16_t)std::stoi(argv[2]);

    RpcClient c(host, port);
    c.connect_server();

    // 1) add(7, 35)
    {
        Response r = c.call("add", { Value::make_int(7), Value::make_int(35) });
        if (r.status == 0 && r.has_result && r.result.type == ValueType::INT64)
            std::cout << "[client] add result = " << r.result.i64 << "\n";
        else
            std::cout << "[client] add error: (" << r.status << ") " << r.err_msg << "\n";
    }

    // 2) echo("hello rpc")
    {
        Response r = c.call("echo", { Value::make_str("hello rpc") });
        if (r.status == 0 && r.has_result && r.result.type == ValueType::STRING)
            std::cout << "[client] echo result = " << r.result.str << "\n";
        else
            std::cout << "[client] echo error: (" << r.status << ") " << r.err_msg << "\n";
    }

    c.close_client();
    return 0;
}
