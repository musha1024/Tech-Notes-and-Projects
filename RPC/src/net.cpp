#include "rpc/net.h"
#include "rpc/protocol.h"
#include <cstring>
#include <iostream>
#include <cstdlib>
#include <limits>

namespace rpc {

[[noreturn]] void die(const std::string& msg){
#ifdef _WIN32
    std::cerr << msg << ", err=" << GET_LAST_ERR() << "\n";
#else
    perror(msg.c_str());
#endif
    std::exit(1);
}

socket_t tcp_listen(uint16_t port){
    if (!net_init()) die("WSAStartup");

    socket_t s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET_T) die("socket");

    int yes = 1;
    if (::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, SOCKOPT_PTR(&yes), sizeof(yes)) == SOCKET_ERROR_T)
        die("setsockopt");

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);

    if (::bind(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR_T) die("bind");
    if (::listen(s, 128) == SOCKET_ERROR_T) die("listen");

    return s;
}

socket_t tcp_connect(const std::string& host, uint16_t port){
    if (!net_init()) die("WSAStartup");

    socket_t s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET_T) die("socket");

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) die("inet_pton");

    if (::connect(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR_T) die("connect");
    return s;
}

void close_fd(socket_t s){
    if (s != INVALID_SOCKET_T) {
        CLOSESOCK(s);
        net_cleanup();
    }
}

void write_n(socket_t s, const void* buf, size_t n){
    const uint8_t* p = static_cast<const uint8_t*>(buf);
    size_t left = n;
    while (left > 0){
        // send 的长度参数在 WinSock 是 int
        int chunk = (left > static_cast<size_t>(std::numeric_limits<int>::max()))
                    ? std::numeric_limits<int>::max()
                    : static_cast<int>(left);
        int w = ::send(s, reinterpret_cast<const char*>(p), chunk, 0);
        if (w == SOCKET_ERROR_T) die("send");
        if (w == 0) die("send returned 0");
        p    += w;
        left -= static_cast<size_t>(w);
    }
}

bool read_n(socket_t s, void* buf, size_t n){
    uint8_t* p = static_cast<uint8_t*>(buf);
    size_t left = n;
    while (left > 0){
        int chunk = (left > static_cast<size_t>(std::numeric_limits<int>::max()))
                    ? std::numeric_limits<int>::max()
                    : static_cast<int>(left);
        int r = ::recv(s, reinterpret_cast<char*>(p), chunk, 0);
        if (r == SOCKET_ERROR_T) die("recv");
        if (r == 0) return false; // 对端关闭
        p    += r;
        left -= static_cast<size_t>(r);
    }
    return true;
}

void send_frame(socket_t s, const std::vector<uint8_t>& frame){
    write_n(s, frame.data(), frame.size());
}

std::optional<RawFrame> recv_frame(socket_t s){
    uint8_t len4[4];
    if (!read_n(s, len4, 4)) return std::nullopt;
    uint32_t body_len = (uint32_t(len4[0])<<24) | (uint32_t(len4[1])<<16)
                      | (uint32_t(len4[2])<<8)  |  uint32_t(len4[3]);

    std::vector<uint8_t> body(body_len);
    if (!read_n(s, body.data(), body.size())) return std::nullopt;

    return parse_body_to_frame(body);
}

} // namespace rpc
