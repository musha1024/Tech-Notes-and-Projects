#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include <cstdint>   // ✅ 为了 uint16_t

class ChatServer {
public:
    explicit ChatServer(uint16_t port);
    ~ChatServer();

    void run();

private:
    int listen_fd_{-1};
    int epfd_{-1};

    struct Client {
        int fd;
        std::string name;
        std::string inbuf;
    };

    std::unordered_map<int, Client> clients_;

    void on_accept();
    void on_readable(int fd);
    void disconnect(int fd, const std::string& reason);

    void broadcast(const std::string& msg);
    bool send_line(int fd, const std::string& line);
};
