#pragma once
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <optional>
#include <sstream>
#include <iostream>

struct RedisClientConfig {
    std::string host = "127.0.0.1";
    uint16_t port = 6379;
    int timeout_ms = 2000;
    std::string password;
};

// Minimal RESP-safe Redis client (no external deps).
// We always send arguments as RESP arrays to avoid command injection.
class RedisClient {
public:
    explicit RedisClient(RedisClientConfig cfg) : cfg_(std::move(cfg)) {}
    ~RedisClient() { close(); }

    void connect() {
        close();
        // Resolve
        struct addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        struct addrinfo* res = nullptr;
        std::string port_str = std::to_string(cfg_.port);
        int err = getaddrinfo(cfg_.host.c_str(), port_str.c_str(), &hints, &res);
        if (err != 0) {
            throw std::runtime_error(std::string("getaddrinfo: ") + gai_strerror(err));
        }

        int sfd_local = -1;
        for (struct addrinfo* rp = res; rp != nullptr; rp = rp->ai_next) {
            sfd_local = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (sfd_local == -1) continue;
            if (::connect(sfd_local, rp->ai_addr, rp->ai_addrlen) == 0) break;
            ::close(sfd_local); sfd_local = -1;
        }
        freeaddrinfo(res);
        if (sfd_local == -1) throw std::runtime_error("Failed to connect to Redis");

        sfd_ = sfd_local;
        buffer_.clear();
    }

    void close() {
        if (sfd_ >= 0) { ::close(sfd_); sfd_ = -1; }
        buffer_.clear();
    }

    bool auth(const std::string &password) {
        auto r = command({"AUTH", password});
        return r.type == Type::SimpleString || r.type == Type::BulkString;
    }

    bool set(const std::string &key, const std::string &value) {
        auto r = command({"SET", key, value});
        return r.type == Type::SimpleString || r.type == Type::BulkString;
    }

    bool setex(const std::string &key, const std::string &value, int ttl_seconds) {
        auto r = command({"SETEX", key, std::to_string(ttl_seconds), value});
        return r.type == Type::SimpleString;
    }

    std::optional<std::string> get(const std::string &key) {
        auto r = command({"GET", key});
        if (r.type == Type::Null) return std::nullopt;
        if (r.type == Type::BulkString) return r.bulk;
        if (r.type == Type::SimpleString) return r.simple;
        return std::nullopt;
    }

    long long incrby(const std::string &key, long long by) {
        auto r = command({"INCRBY", key, std::to_string(by)});
        if (r.type != Type::Integer) throw std::runtime_error("INCRBY did not return integer");
        return r.integer;
    }

    long long del(const std::string &key) {
        auto r = command({"DEL", key});
        if (r.type != Type::Integer) throw std::runtime_error("DEL did not return integer");
        return r.integer;
    }

    bool exists(const std::string &key) {
        auto r = command({"EXISTS", key});
        if (r.type != Type::Integer) throw std::runtime_error("EXISTS did not return integer");
        return r.integer > 0;
    }

private:
    enum class Type { SimpleString, Error, Integer, BulkString, Array, Null };

    struct Reply {
        Type type;
        std::string simple;
        std::string error;
        long long integer = 0;
        std::string bulk;
        std::vector<Reply> array;
    };

    Reply command(const std::vector<std::string> &args) {
        if (sfd_ < 0) throw std::runtime_error("Redis not connected");
        std::string payload = build_resp_array(args);
        if (!send_all(payload)) throw std::runtime_error("send failed");
        return parse_reply();
    }

    static std::string build_resp_array(const std::vector<std::string> &args) {
        std::ostringstream oss;
        oss << "*" << args.size() << "\r\n";
        for (auto &a : args) {
            oss << "$" << a.size() << "\r\n" << a << "\r\n";
        }
        return oss.str();
    }

    bool send_all(const std::string &p) {
        size_t sent = 0;
        while (sent < p.size()) {
            ssize_t n = ::send(sfd_, p.data() + sent, p.size() - sent, 0);
            if (n < 0) {
                if (errno == EINTR) continue;
                return false;
            }
            sent += static_cast<size_t>(n);
        }
        return true;
    }

    bool read_until(size_t n) {
        while (buffer_.size() < n) {
            char tmp[4096];
            ssize_t r = ::recv(sfd_, tmp, sizeof(tmp), 0);
            if (r <= 0) return false;
            buffer_.append(tmp, r);
            if (buffer_.size() > (1<<24)) return false; // 16MB cap
        }
        return true;
    }

    bool read_line(std::string &line) {
        size_t pos = buffer_.find("\r\n");
        while (pos == std::string::npos) {
            char tmp[4096];
            ssize_t r = ::recv(sfd_, tmp, sizeof(tmp), 0);
            if (r <= 0) return false;
            buffer_.append(tmp, r);
            pos = buffer_.find("\r\n");
            if (buffer_.size() > (1<<24)) return false;
        }
        line = buffer_.substr(0, pos);
        buffer_.erase(0, pos + 2);
        return true;
    }

    Reply parse_reply() {
        // Reply starts with one byte indicating type
        std::string line;
        if (!read_line(line)) throw std::runtime_error("redis: failed to read reply line");
        if (line.empty()) throw std::runtime_error("redis: empty reply");

        char t = line[0];
        switch (t) {
            case '+': { // Simple String
                Reply r{Type::SimpleString};
                r.simple = line.substr(1);
                return r;
            }
            case '-': { // Error
                Reply r{Type::Error};
                r.error = line.substr(1);
                throw std::runtime_error(std::string("redis error: ") + r.error);
            }
            case ':': { // Integer
                Reply r{Type::Integer};
                r.integer = std::stoll(line.substr(1));
                return r;
            }
            case '$': { // Bulk String
                long long len = std::stoll(line.substr(1));
                if (len == -1) {
                    Reply r{Type::Null};
                    return r;
                }
                if (!read_until(static_cast<size_t>(len + 2))) throw std::runtime_error("redis: bulk short read");
                Reply r{Type::BulkString};
                r.bulk = buffer_.substr(0, static_cast<size_t>(len));
                buffer_.erase(0, static_cast<size_t>(len + 2)); // remove bulk and CRLF
                return r;
            }
            case '*': { // Array
                long long n = std::stoll(line.substr(1));
                if (n == -1) {
                    Reply r{Type::Null};
                    return r;
                }
                Reply r{Type::Array};
                r.array.reserve(static_cast<size_t>(n));
                for (long long i = 0; i < n; ++i) {
                    r.array.push_back(parse_reply());
                }
                return r;
            }
            default:
                throw std::runtime_error("redis: unknown reply type");
        }
    }

    RedisClientConfig cfg_;
    int sfd_ = -1;
    std::string buffer_;
};
