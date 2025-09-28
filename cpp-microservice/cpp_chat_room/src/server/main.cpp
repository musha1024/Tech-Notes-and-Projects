#include "server/ChatServer.hpp"
#include <iostream>
#include <cstdlib>

int main(int argc, char** argv){
    uint16_t port = 7777;
    if (argc >= 2) port = static_cast<uint16_t>(std::atoi(argv[1]));

    try {
        ChatServer s(port);
        s.run();
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}