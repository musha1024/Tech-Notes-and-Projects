#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "rpc/frame.h"

/**
 * net：负责 socket 连接、可靠读写（处理短读短写）、按长度前缀收发整帧。
 * 仅提供 POSIX/Linux 实现。Windows 需改为 WinSock 初始化/调用。
 */
namespace rpc {

[[noreturn]] void die(const std::string& msg);

int tcp_listen(uint16_t port);
int tcp_connect(const std::string& host, uint16_t port);
void close_fd(int fd);

void write_n(int fd, const void* buf, size_t n);
bool read_n(int fd, void* buf, size_t n);

// 发送整帧（已经 build_*_frame 好的字节）
void send_frame(int fd, const std::vector<uint8_t>& frame);

// 接收一帧：先读4B长度，再读 body，返回 RawFrame（对端关闭返回 nullopt）
std::optional<RawFrame> recv_frame(int fd);

} // namespace rpc
