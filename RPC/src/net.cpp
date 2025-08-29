#include "rpc/net.h"
#include "rpc/protocol.h"
#include <cstring>
#include <iostream>
#include <cstdlib>
#include <limits>

namespace rpc {

// =====================================================
// die(msg): 致命错误处理函数
//   - 打印错误信息并退出程序
//   - Windows 下用 WSAGetLastError() 打印错误码
//   - Linux 下直接用 perror
// =====================================================
[[noreturn]] void die(const std::string& msg){
#ifdef _WIN32
    std::cerr << msg << ", err=" << GET_LAST_ERR() << "\n";
#else
    perror(msg.c_str());
#endif
    std::exit(1);
}

// =====================================================
// tcp_listen(port):
//   - 在给定端口上创建 TCP 监听 socket
//   - 设置 SO_REUSEADDR 以支持端口快速复用
//   - bind 到 0.0.0.0:port
//   - listen 队列大小 128
//
// 输入:  port 要监听的端口号
// 输出:  成功返回 socket_t（Linux=int，Windows=SOCKET）
// 错误:  调用 die() 直接退出
// =====================================================
socket_t tcp_listen(uint16_t port){
    if (!net_init()) die("WSAStartup");

    socket_t s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET_T) die("socket");

    int yes = 1;
    if (::setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
                     SOCKOPT_PTR(&yes), sizeof(yes)) == SOCKET_ERROR_T)
        die("setsockopt");

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);

    if (::bind(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR_T) die("bind");
    if (::listen(s, 128) == SOCKET_ERROR_T) die("listen");

    return s;
}

// =====================================================
// tcp_connect(host, port):
//   - 主动连接到指定 host:port 的 TCP 服务端
//
// 输入:  host 字符串形式的 IPv4 地址，port 端口号
// 输出:  成功返回 socket_t
// 错误:  调用 die() 直接退出
// =====================================================
socket_t tcp_connect(const std::string& host, uint16_t port){
    if (!net_init()) die("WSAStartup");

    socket_t s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET_T) die("socket");

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0)
        die("inet_pton");

    if (::connect(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR_T)
        die("connect");
    return s;
}

// =====================================================
// close_fd(s):
//   - 关闭 socket
//   - Windows 下还会调用 WSACleanup()
// =====================================================
void close_fd(socket_t s){
    if (s != INVALID_SOCKET_T) {
        CLOSESOCK(s);
        net_cleanup();
    }
}

// =====================================================
// write_n(s, buf, n):
//   - 向 socket 写入恰好 n 字节
//   - 内部循环处理短写（send 可能只写部分）
//   - Windows 下 send 的长度参数是 int，所以分块处理
//
// 输入:  s   socket
//        buf 待发送的数据指针
//        n   待发送字节数
// 输出:  无返回（成功保证写完）
// 错误:  调用 die() 或退出
// =====================================================
void write_n(socket_t s, const void* buf, size_t n){
    const uint8_t* p = static_cast<const uint8_t*>(buf);
    size_t left = n;
    while (left > 0){
        int chunk = (left > static_cast<size_t>(std::numeric_limits<int>::max()))
                    ? std::numeric_limits<int>::max()
                    : static_cast<int>(left);
        int w = ::send(s, reinterpret_cast<const char*>(p), chunk, 0);
        if (w == SOCKET_ERROR_T) die("send");
        if (w == 0) die("send returned 0"); // 意外
        p    += w;
        left -= static_cast<size_t>(w);
    }
}

// =====================================================
// read_n(s, buf, n):
//   - 从 socket 读取恰好 n 字节
//   - 内部循环处理短读（recv 可能只读部分）
//   - 如果对端关闭连接，返回 false
//
// 输入:  s   socket
//        buf 缓冲区指针
//        n   期望读取的字节数
// 输出:  true = 成功读满
//        false = 对端关闭连接
// 错误:  调用 die() 或退出
// =====================================================
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

// =====================================================
// send_frame(s, frame):
//   - 发送完整的一帧（frame 已经包含 4B 长度前缀）
// =====================================================
void send_frame(socket_t s, const std::vector<uint8_t>& frame){
    write_n(s, frame.data(), frame.size());
}

// =====================================================
// recv_frame(s):
//   - 从 socket 上读取一帧：
//       1) 先读 4 字节长度
//       2) 再读 body_len 个字节
//       3) 调用 parse_body_to_frame() 转换成 RawFrame
//   - 如果对端关闭连接，返回 std::nullopt
//
// 输出:  RawFrame 对象（含 type、req_id、method、payload）
// =====================================================
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
