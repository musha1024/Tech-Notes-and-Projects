#include "client/ChatClient.hpp"
#include "common/net.hpp"
#include <iostream>
#include <vector>

ChatClient::ChatClient(const std::string& ip, uint16_t port, const std::string& nickname)
: ip_(ip), port_(port), nickname_(nickname) {}

ChatClient::~ChatClient() {
    stop_ = true;
    if (sock_ >= 0) ::close(sock_);
    if (recv_th_.joinable()) recv_th_.join();
}

void ChatClient::recv_loop() {
    std::string buf;
    buf.reserve(4096);
    char tmp[4096];

    while (!stop_) {
        ssize_t n = :: recv(sock_, tmp, sizeof(tmp), 0);

        if(n > 0) {
            std::cout.write(tmp, n);
            std::cout.flush();
        } else if (n == 0) {
            std::cout << "\n[INFO] Server closed the connection.\n";
            stop_ = true; break;
        } else {
            if (errno == EINTR) continue;
            std::cout << "\n[ERROR] recv: " << net::errno_str() << "\n";
            stop_ = true; break;
        }

    }

}

void ChatClient::run() {
    sock_ = net::create_client_fd(ip_, port_);
    if (sock_ < 0) throw std::runtime_error("connect failed: " + net::errno_str());

    std::string hello = nickname_ + "\n";
    net::send_all(sock_, hello.data(), hello.size());

    recv_th_ = std::thread([this]{ this->recv_loop(); });

    std::string line;
    while (!stop_ && std::getline(std::cin, line)) {
        line.push_back('\n');
        if (!net::send_all(sock_, line.data(), line.size())) {
            std::cout << "[ERROR] send failed\n";
            break;
        }
    }


    stop_ = true;
}