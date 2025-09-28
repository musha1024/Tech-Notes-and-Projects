#include "server/ChatServer.hpp"
#include "common/net.hpp"
#include <iostream>
#include <sstream>
#include <vector>        // ✅ 用到了 std::vector
#include <stdexcept>     // ✅ 用到了 std::runtime_error

ChatServer::ChatServer(uint16_t port) {
    listen_fd_ = net::create_server_fd(port);   // ✅ 修正拼写
    if (listen_fd_ < 0) {
        throw std::runtime_error("Create server fd failed: " + net::errno_str());
    }

    net::set_nonblock(listen_fd_);

    epfd_ = ::epoll_create1(0);
    if (epfd_ < 0) throw std::runtime_error("epoll_create1 failed");

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd_;

    if (::epoll_ctl(epfd_, EPOLL_CTL_ADD, listen_fd_, &ev) < 0) {
        throw std::runtime_error("epoll_ctl ADD listen failed");  // ✅
    }
    std::cout << "[Server] Listening on port " << port << "...\n";
}

ChatServer::~ChatServer() {
    if (listen_fd_ >= 0) ::close(listen_fd_);
    if (epfd_ >= 0) ::close(epfd_);
}

void ChatServer::run() {
    constexpr int MAX_EVENTS = 128;
    std::vector<epoll_event> events(MAX_EVENTS);

    while (true) {
        int n = ::epoll_wait(epfd_, events.data(), MAX_EVENTS, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            std::cerr << "epoll_wait error: " << net::errno_str() << "\n";
            break;
        }

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            uint32_t ev = events[i].events;
            if (fd == listen_fd_) {
                on_accept();
            } else if (ev & (EPOLLIN | EPOLLHUP | EPOLLERR)) {
                on_readable(fd);
            }
        }
    }
}

void ChatServer::on_accept() {
    while (true) {
        sockaddr_in cliaddr{};
        socklen_t len = sizeof(cliaddr);
        int cfd = ::accept(listen_fd_, (sockaddr*)&cliaddr, &len);

        if (cfd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return; // 非阻塞已取尽
            std::cerr << "accept error: " << net::errno_str() << "\n";
            return;
        }

        net::set_nonblock(cfd);

        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = cfd;
        if (::epoll_ctl(epfd_, EPOLL_CTL_ADD, cfd, &ev) < 0) {
            std::cerr << "epoll_ctl ADD client failed\n";
            ::close(cfd);
            continue;
        }

        clients_[cfd] = Client{cfd, /*name*/"", /*inbuf*/""};
        send_line(cfd, "Welcome! Please type your nickname on the first line.\n");
    }
}

void ChatServer::on_readable(int fd) {
    auto it = clients_.find(fd);
    if (it == clients_.end()) {
        disconnect(fd, "not in clients");
        return;
    }
    Client& cli = it->second;

    char buf[4096];
    while (true) {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n > 0) {
            cli.inbuf.append(buf, buf + n);
        } else if (n == 0) {
            std::string nn = cli.name.empty() ? ("#" + std::to_string(fd)) : cli.name;
            broadcast("[INFO] " + nn + " left the chat.\n");
            disconnect(fd, "peer closed");
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break; // 本轮读完
            disconnect(fd, "recv error");
            return;
        }
    }

    // 按行协议解析
    size_t pos = 0;
    while (true) {
        size_t nl = cli.inbuf.find('\n', pos);
        if (nl == std::string::npos) {
            // 不完整行：丢弃已处理的前缀，保留尾部碎片
            cli.inbuf.erase(0, pos);
            break;
        }
        std::string line = cli.inbuf.substr(pos, nl - pos);
        pos = nl + 1;

        if (!line.empty() && line.back() == '\r') line.pop_back(); // 去 '\r'

        if (cli.name.empty()) {
            cli.name = line.empty() ? ("#" + std::to_string(fd)) : line;
            broadcast("[INFO] " + cli.name + " joined the chat.\n");
            continue;
        }

        std::string msg = "[" + cli.name + "] " + line + "\n";
        broadcast(msg);
    }
}

void ChatServer::disconnect(int fd, const std::string&) {
    ::epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
    ::close(fd);
    clients_.erase(fd);
}

bool ChatServer::send_line(int fd, const std::string& line) {
    return net::send_all(fd, line.data(), line.size());
}

void ChatServer::broadcast(const std::string& msg) {
    std::vector<int> to_close;
    for (auto& [fd, _] : clients_) {          // ✅ 去掉未用变量告警
        if (!send_line(fd, msg)) {
            to_close.push_back(fd);
        }
    }
    for (int fd : to_close) {
        disconnect(fd, "send fail");
    }
    std::cout << msg;
    std::cout.flush();
}
