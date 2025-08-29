#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "rpc/frame.h"

// --- 平台差异封装：类型、初始化、关闭、错误码 ---
#ifdef _WIN32
  #define NOMINMAX
  #include <winsock2.h>
  #include <ws2tcpip.h>   // inet_pton/getaddrinfo
  #pragma comment(lib, "ws2_32.lib")
  using socket_t = SOCKET;
  #define INVALID_SOCKET_T INVALID_SOCKET
  #define SOCKET_ERROR_T   SOCKET_ERROR
  #define CLOSESOCK(s)     ::closesocket(s)
  #define GET_LAST_ERR()   ::WSAGetLastError()
  inline bool net_init()   { WSADATA w; return ::WSAStartup(MAKEWORD(2,2), &w) == 0; }
  inline void net_cleanup(){ ::WSACleanup(); }
  // Windows 下 setsockopt 的指针类型通常是 const char*
  #define SOCKOPT_PTR(p)   (const char*)(p)
#else
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <sys/socket.h>
  #include <sys/types.h>
  #include <unistd.h>
  #include <errno.h>
  using socket_t = int;
  #define INVALID_SOCKET_T (-1)
  #define SOCKET_ERROR_T   (-1)
  #define CLOSESOCK(s)     ::close(s)
  #define GET_LAST_ERR()   (errno)
  inline bool net_init()   { return true; }
  inline void net_cleanup(){ }
  #define SOCKOPT_PTR(p)   (const void*)(p)
#endif

/**
 * net：负责 socket 连接、可靠读写（处理短读短写）、按长度前缀收发整帧。
 * 现在支持 Linux + Windows（WinSock）。
 */
namespace rpc {

[[noreturn]] void die(const std::string& msg);

// 建立监听/连接：返回 socket_t（Windows 是 SOCKET，Linux 是 int）
socket_t tcp_listen(uint16_t port);
socket_t tcp_connect(const std::string& host, uint16_t port);

void close_fd(socket_t s);

// 可靠写/读 n 字节
void write_n(socket_t s, const void* buf, size_t n);
bool read_n(socket_t s, void* buf, size_t n);

// 发送/接收一帧（4B 大端长度 + body）
void send_frame(socket_t s, const std::vector<uint8_t>& frame);
std::optional<RawFrame> recv_frame(socket_t s);

} // namespace rpc
