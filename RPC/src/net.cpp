#include "rpc/net.h"
#include "rpc/protocol.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

namespace rpc {

[[noreturn]] void die(const std::string& msg){
    perror(msg.c_str());
    std::exit(1);
}

int tcp_listen(uint16_t port){
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) die("socket");

    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
        die("setsockopt");

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0) die("bind");
    if (listen(fd, 128) < 0) die("listen");
    return fd;
}

int tcp_connect(const std::string& host, uint16_t port){
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) die("socket");

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) die("inet_pton");

    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) die("connect");
    return fd;
}

void close_fd(int fd){
    if (fd >= 0) ::close(fd);
}

void write_n(int fd, const void* buf, size_t n){
    const uint8_t* p = static_cast<const uint8_t*>(buf);
    size_t left = n;
    while (left > 0){
        ssize_t w = ::send(fd, p, left, 0);
        if (w < 0) die("send");
        if (w == 0) die("send returned 0");
        p += w;
        left -= (size_t)w;
    }
}

bool read_n(int fd, void* buf, size_t n){
    uint8_t* p = static_cast<uint8_t*>(buf);
    size_t left = n;
    while (left > 0){
        ssize_t r = ::recv(fd, p, left, 0);
        if (r < 0) die("recv");
        if (r == 0) return false;
        p += r;
        left -= (size_t)r;
    }
    return true;
}

void send_frame(int fd, const std::vector<uint8_t>& frame){
    write_n(fd, frame.data(), frame.size());
}

std::optional<RawFrame> recv_frame(int fd){
    uint8_t len4[4];
    if (!read_n(fd, len4, 4)) return std::nullopt;
    uint32_t body_len = (uint32_t(len4[0])<<24)|(uint32_t(len4[1])<<16)|(uint32_t(len4[2])<<8)|uint32_t(len4[3]);

    std::vector<uint8_t> body(body_len);
    if (!read_n(fd, body.data(), body.size())) return std::nullopt;

    return parse_body_to_frame(body);
}

} // namespace rpc