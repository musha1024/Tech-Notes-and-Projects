#pragma once
#include <string>
#include <vector>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


namespace net {

inline int set_nonblock(int fd) {
    int flags = fcntl(fd , F_GETFL, 0);
    if (flags == -1) return -1;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) return -1;
    return 0;
}

inline int create_server_fd(uint16_t port , int backlog = 128) {
    int fd = ::socket(AF_INET, SOCK_STREAM , 0);
    if (fd < 0) return -1;
    int yes = 1;
    ::setsockopt(fd, SOL_SOCKET , SO_REUSEADDR , &yes , sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if(::bind(fd , (sockaddr*)&addr, sizeof(addr)) < 0) {
        ::close(fd);
        return -1;
    }

    if(::listen(fd , backlog) < 0) {
        ::close(fd);
        return -1;
    }

    return fd;
}


inline int create_client_fd(const std::string& ip, uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM , 0);
    if (fd < 0) return -1;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if(::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1){
        ::close(fd);
        return -1;
    }

    if(::connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0){
        ::close(fd);
        return -1;
    }

    return fd;

}

inline bool send_all(int fd, const void* buf , size_t len){
    const char* p = static_cast<const char*>(buf);
    size_t left = len;

    if(left == 0) return true;

    while(left > 0) {
        ssize_t n = ::send(fd, p, left , 0);
        
        if(n > 0) {
            p += n;
            left -= static_cast<size_t>(n);
            continue;
        }

        if(n == 0) {
            return false;
        }

        if(errno == EINTR){
            continue;
        }

        if(errno == EAGAIN || errno == EWOULDBLOCK){
            return false;
        }

        return false;

    }

    return true;
}


inline std::string errno_str() { return std::string(strerror(errno)); }
}