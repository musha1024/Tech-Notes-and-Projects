#pragma once
#include <string>
#include <thread>
#include <atomic>

class ChatClient {
public:
    ChatClient(const std::string& ip, uint16_t port, const std::string& nikename);
    ~ChatClient();

    void run();

private:
    std::string ip_;
    uint16_t port_{};
    std::string nickname_;

    int sock_{-1};
    std::thread recv_th_;
    std::atomic<bool> stop_{false};

    void recv_loop();

};