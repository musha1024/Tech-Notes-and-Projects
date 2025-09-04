#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "redis_client.hpp"

namespace http {

struct Request {
    std::string method;
    std::string path;
    std::unordered_map<std::string, std::string> query;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

struct Response {
    int status = 200;
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    std::string to_string() const {
        std::ostringstream oss;
        oss << "HTTP/1.1 " << status << " " << reason_phrase(status) << "\r\n";
        for (auto &kv : headers) {
            oss << kv.first << ": " << kv.second << "\r\n";
        }
        oss << "Content-Length: " << body.size() << "\r\n";
        oss << "Connection: close\r\n\r\n";
        oss << body;
        return oss.str();
    }

    static const char* reason_phrase(int status) {
        switch (status) {
            case 200: return "OK";
            case 201: return "Created";
            case 204: return "No Content";
            case 400: return "Bad Request";
            case 401: return "Unauthorized";
            case 403: return "Forbidden";
            case 404: return "Not Found";
            case 405: return "Method Not Allowed";
            case 413: return "Payload Too Large";
            case 429: return "Too Many Requests";
            case 500: return "Internal Server Error";
            default: return "Unknown";
        }
    }
};

static std::string url_decode(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            std::string hex = s.substr(i+1, 2);
            char ch = static_cast<char>(strtol(hex.c_str(), nullptr, 16));
            out.push_back(ch);
            i += 2;
        } else if (s[i] == '+') {
            out.push_back(' ');
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

static std::unordered_map<std::string, std::string> parse_query(const std::string &qs) {
    std::unordered_map<std::string, std::string> m;
    size_t start = 0;
    while (start < qs.size()) {
        size_t amp = qs.find('&', start);
        if (amp == std::string::npos) amp = qs.size();
        std::string pair = qs.substr(start, amp - start);
        size_t eq = pair.find('=');
        if (eq != std::string::npos) {
            std::string k = url_decode(pair.substr(0, eq));
            std::string v = url_decode(pair.substr(eq + 1));
            m[k] = v;
        } else if (!pair.empty()) {
            m[url_decode(pair)] = "";
        }
        start = amp + 1;
    }
    return m;
}

static bool read_line(int fd, std::string &out_line, std::string &buffer) {
    size_t pos;
    while ((pos = buffer.find("\r\n")) == std::string::npos) {
        char tmp[4096];
        ssize_t n = ::recv(fd, tmp, sizeof(tmp), 0);
        if (n <= 0) return false;
        buffer.append(tmp, n);
        if (buffer.size() > 1<<20) return false; // 1MB header cap
    }
    out_line = buffer.substr(0, pos);
    buffer.erase(0, pos + 2);
    return true;
}

static bool read_exact(int fd, std::string &buffer, size_t bytes) {
    while (buffer.size() < bytes) {
        char tmp[8192];
        ssize_t n = ::recv(fd, tmp, sizeof(tmp), 0);
        if (n <= 0) return false;
        buffer.append(tmp, n);
        if (buffer.size() > (1<<24)) return false; // 16MB cap
    }
    return true;
}

static bool parse_request(int fd, Request &req) {
    std::string buf;
    std::string line;
    if (!read_line(fd, line, buf)) return false;

    // Request line: METHOD SP PATH?QUERY SP HTTP/x.x
    {
        std::istringstream iss(line);
        if (!(iss >> req.method)) return false;
        std::string path_query;
        if (!(iss >> path_query)) return false;
        size_t qpos = path_query.find('?');
        if (qpos == std::string::npos) {
            req.path = path_query;
        } else {
            req.path = path_query.substr(0, qpos);
            req.query = parse_query(path_query.substr(qpos + 1));
        }
    }

    // Headers
    while (true) {
        if (!read_line(fd, line, buf)) return false;
        if (line.empty()) break;
        size_t colon = line.find(':');
        if (colon == std::string::npos) return false;
        std::string k = line.substr(0, colon);
        std::string v = line.substr(colon + 1);
        // trim leading spaces
        size_t start = v.find_first_not_of(" \t");
        if (start != std::string::npos) v = v.substr(start);
        req.headers[k] = v;
    }

    // Body
    size_t content_length = 0;
    auto it = req.headers.find("Content-Length");
    if (it != req.headers.end()) {
        content_length = static_cast<size_t>(std::stoul(it->second));
        if (content_length > (1<<22)) return false; // 4MB limit
    }
    if (content_length > 0) {
        if (!read_exact(fd, buf, content_length)) return false;
        req.body = buf.substr(0, content_length);
        buf.erase(0, content_length);
    }
    return true;
}

static std::string json_escape(const std::string &s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if ((unsigned char)c < 0x20) {
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else out += c;
        }
    }
    return out;
}

static std::optional<long long> parse_json_int(const std::string &body, const std::string &key) {
    std::regex re("\"" + key + R"(\"\s*:\s*(-?\d+))");
    std::smatch m;
    if (std::regex_search(body, m, re)) {
        try {
            return std::stoll(m[1].str());
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

static std::optional<std::string> parse_json_string(const std::string &body, const std::string &key) {
    std::regex re("\"" + key + R"(\"\s*:\s*\"((?:\\.|[^\\\"])*)\")");
    std::smatch m;
    if (std::regex_search(body, m, re)) {
        // unescape only \" and \\ for simplicity
        std::string s = m[1].str();
        std::string out;
        out.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                char c = s[++i];
                if (c == '\\' || c == '"') out.push_back(c);
                else if (c == 'n') out.push_back('\n');
                else if (c == 'r') out.push_back('\r');
                else if (c == 't') out.push_back('\t');
                else out.push_back(c);
            } else out.push_back(s[i]);
        }
        return out;
    }
    return std::nullopt;
}

// Basic key validation to avoid resource abuse; RESP makes injection unlikely.
static bool valid_key(const std::string &k) {
    if (k.empty() || k.size() > 128) return false;
    static const std::regex re("^[A-Za-z0-9:_\\-]+$");
    return std::regex_match(k, re);
}

class Server {
public:
    Server(std::string host, uint16_t port, RedisClientConfig rcfg)
        : host_(std::move(host)), port_(port), rcfg_(std::move(rcfg)) {}

    void run() {
        int sfd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (sfd < 0) throw std::runtime_error("socket failed");
        int yes = 1;
        setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port_);
        if (::bind(sfd, (sockaddr*)&addr, sizeof(addr)) < 0) {
            ::close(sfd);
            throw std::runtime_error("bind failed (need permission? busy port?)");
        }
        if (::listen(sfd, 128) < 0) {
            ::close(sfd);
            throw std::runtime_error("listen failed");
        }

        std::cerr << "[http] Listening on 0.0.0.0:" << port_ << "\n";

        while (true) {
            sockaddr_in caddr{};
            socklen_t clen = sizeof(caddr);
            int cfd = ::accept(sfd, (sockaddr*)&caddr, &clen);
            if (cfd < 0) {
                if (errno == EINTR) continue;
                std::cerr << "accept error: " << strerror(errno) << "\n";
                continue;
            }
            std::thread(&Server::handle_client, this, cfd).detach();
        }
    }

private:
    void handle_client(int cfd) {
        http::Request req;
        if (!parse_request(cfd, req)) {
            http::Response res;
            res.status = 400;
            res.headers["Content-Type"] = "application/json";
            res.body = R"({"error":"bad request"})";
            std::string out = res.to_string();
            ::send(cfd, out.data(), out.size(), 0);
            ::close(cfd);
            return;
        }

        http::Response res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";

        try {
            if (req.method == "GET" && req.path == "/ping") {
                res.body = R"({"ok":true})";
            } else if (req.method == "GET" && req.path == "/get") {
                auto it = req.query.find("key");
                if (it == req.query.end() || !valid_key(it->second)) {
                    res.status = 400;
                    res.body = R"({"error":"invalid or missing key"})";
                } else {
                    ensure_redis();
                    auto v = redis_->get(it->second);
                    if (v.has_value())
                        res.body = std::string("{\"key\":\"") + json_escape(it->second) + "\",\"value\":\"" + json_escape(*v) + "\"}";
                    else
                        res.body = std::string("{\"key\":\"") + json_escape(it->second) + "\",\"value\":null}";
                }
            } else if (req.method == "POST" && req.path == "/set") {
                auto k = parse_json_string(req.body, "key");
                auto v = parse_json_string(req.body, "value");
                auto ttl = parse_json_int(req.body, "ttl"); // seconds
                if (!k || !v || !valid_key(*k)) {
                    res.status = 400;
                    res.body = R"({"error":"invalid key or value"})";
                } else {
                    if (v->size() > (1<<20)) { // 1MB value cap
                        res.status = 413;
                        res.body = R"({"error":"value too large"})";
                    } else {
                        ensure_redis();
                        bool ok = false;
                        if (ttl && *ttl > 0) ok = redis_->setex(*k, *v, static_cast<int>(*ttl));
                        else ok = redis_->set(*k, *v);
                        res.body = std::string("{\"ok\":") + (ok ? "true" : "false") + "}";
                    }
                }
            } else if (req.method == "DELETE" && req.path == "/del") {
                auto it = req.query.find("key");
                if (it == req.query.end() || !valid_key(it->second)) {
                    res.status = 400;
                    res.body = R"({"error":"invalid or missing key"})";
                } else {
                    ensure_redis();
                    long long n = redis_->del(it->second);
                    res.body = std::string("{\"deleted\":") + std::to_string(n) + "}";
                }
            } else if (req.method == "POST" && req.path == "/incr") {
                auto k = parse_json_string(req.body, "key");
                auto by = parse_json_int(req.body, "by");
                if (!k || !valid_key(*k)) {
                    res.status = 400;
                    res.body = R"({"error":"invalid key"})";
                } else {
                    ensure_redis();
                    long long delta = by.value_or(1);
                    long long newv = redis_->incrby(*k, delta);
                    res.body = std::string("{\"new\":") + std::to_string(newv) + "}";
                }
            } else if (req.method == "GET" && req.path == "/exists") {
                auto it = req.query.find("key");
                if (it == req.query.end() || !valid_key(it->second)) {
                    res.status = 400;
                    res.body = R"({"error":"invalid or missing key"})";
                } else {
                    ensure_redis();
                    bool ex = redis_->exists(it->second);
                    res.body = std::string("{\"exists\":") + (ex ? "true" : "false") + "}";
                }
            } else {
                res.status = 404;
                res.body = R"({"error":"not found"})";
            }
        } catch (const std::exception &e) {
            res.status = 500;
            res.body = std::string("{\"error\":\"") + json_escape(e.what()) + "\"}";
        }

        std::string out = res.to_string();
        ::send(cfd, out.data(), out.size(), 0);
        ::close(cfd);
    }

    void ensure_redis() {
        std::call_once(redis_once_, [&](){
            redis_ = std::make_unique<RedisClient>(rcfg_);
            redis_->connect();
            if (!rcfg_.password.empty()) {
                if (!redis_->auth(rcfg_.password)) throw std::runtime_error("Redis AUTH failed");
            }
        });
    }

    std::string host_;
    uint16_t port_;
    RedisClientConfig rcfg_;
    std::once_flag redis_once_;
    std::unique_ptr<RedisClient> redis_;
};

} // namespace http

static bool g_stop = false;
static void on_sigint(int) { g_stop = true; }

int main(int argc, char** argv) {
    std::string http_host = "0.0.0.0";
    uint16_t http_port = 8080;
    std::string redis_host = "127.0.0.1";
    uint16_t redis_port = 6379;
    std::string redis_password;
    int redis_timeout_ms = 2000;

    // Simple arg parse
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need = [&](const char* name){ if (i+1 >= argc) { std::cerr << "Missing value for " << name << "\n"; std::exit(1);} return std::string(argv[++i]); };
        if (a == "--port") http_port = static_cast<uint16_t>(std::stoi(need("--port")));
        else if (a == "--host") http_host = need("--host");
        else if (a == "--redis-host") redis_host = need("--redis-host");
        else if (a == "--redis-port") redis_port = static_cast<uint16_t>(std::stoi(need("--redis-port")));
        else if (a == "--redis-password") redis_password = need("--redis-password");
        else if (a == "--redis-timeout-ms") redis_timeout_ms = std::stoi(need("--redis-timeout-ms"));
        else if (a == "--help" || a == "-h") {
            std::cout << "Usage: " << argv[0] << " [--port 8080] [--host 0.0.0.0] [--redis-host 127.0.0.1] [--redis-port 6379] [--redis-password ***] [--redis-timeout-ms 2000]\n";
            return 0;
        }
    }

    // Env overrides
    if (const char* p = std::getenv("PORT")) http_port = static_cast<uint16_t>(std::stoi(p));
    if (const char* p = std::getenv("REDIS_HOST")) redis_host = p;
    if (const char* p = std::getenv("REDIS_PORT")) redis_port = static_cast<uint16_t>(std::stoi(p));
    if (const char* p = std::getenv("REDIS_PASSWORD")) redis_password = p;

    RedisClientConfig rcfg;
    rcfg.host = redis_host;
    rcfg.port = redis_port;
    rcfg.timeout_ms = redis_timeout_ms;
    rcfg.password = redis_password;

    signal(SIGINT, on_sigint);
    try {
        http::Server server(http_host, http_port, rcfg);
        server.run();
    } catch (const std::exception &e) {
        std::cerr << "fatal: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
