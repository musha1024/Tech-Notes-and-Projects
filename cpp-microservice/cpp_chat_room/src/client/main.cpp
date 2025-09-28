#include "client/ChatClient.hpp"
#include <iostream>
#include <cstdlib>

int main(int arg , char** argv) {
    if(arg < 4) {
        std::cerr << "Usage: " << argv[0] << " <server_ip> <port> <nickname>\n";
        return 1;
    }

    std::string ip = argv[1];
    uint16_t port = static_cast<uint16_t>(std::atoi(argv[2]));
    std::string nickname = argv[3];

    try {
        ChatClient c(ip, port, nickname);
        c.run();
    } catch (const std::exception& e) {
        std::cerr << "Client error:" << e.what() << "\n";
        return 1;
    }
    return 0;
}