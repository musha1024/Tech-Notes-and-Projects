#pragma once
#include <hiredis/hiredis.h>
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <stdexcept>
#include <chrono>
#include <thread>

class RedisClient {
public:
    RedisClient(const std::string& host, int port, int db, int timeout_ms=2000)
        : host_(host), port_(port), db_(db), timeout_ms_(timeout_ms) {
        connect();
    }
    ~RedisClient() { disconnect(); }

    bool get(const std::string& key, std::string& value) {
        std::lock_guard<std::mutex> lk(mu_);
        ensure();
        const char* argv[2]; size_t lens[2];
        argv[0] = "GET"; lens[0] = 3;
        argv[1] = key.data(); lens[1] = key.size();
        redisReply* r = (redisReply*)redisCommandArgv(ctx_, 2, argv, lens);
        if(!r){ reconnect(); return false; }
        std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(r, freeReplyObject);
        if(r->type == REDIS_REPLY_NIL) return false;
        if(r->type == REDIS_REPLY_STRING){
            value.assign(r->str, r->len);
            return true;
        }
        return false;
    }

    bool setex(const std::string& key, const std::string& val, int ttl_sec) {
        std::lock_guard<std::mutex> lk(mu_);
        ensure();
        const char* argv[4]; size_t lens[4];
        argv[0] = "SETEX"; lens[0] = 5;
        argv[1] = key.data(); lens[1] = key.size();
        std::string ttl = std::to_string(ttl_sec);
        argv[2] = ttl.data(); lens[2] = ttl.size();
        argv[3] = val.data(); lens[3] = val.size();
        redisReply* r = (redisReply*)redisCommandArgv(ctx_, 4, argv, lens);
        if(!r){ reconnect(); return false; }
        std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(r, freeReplyObject);
        return r->type == REDIS_REPLY_STATUS || r->type == REDIS_REPLY_STRING;

    }

    bool del(const std::string& key) {
        std::lock_guard<std::mutex> lk(mu_);
        ensure();
        const char* argv[2]; size_t lens[2];
        argv[0] = "DEL"; lens[0] = 3;
        argv[1] = key.data(); lens[1] = key.size();
        redisReply* r = (redisReply*)redisCommandArgv(ctx_, 2, argv, lens);
        if(!r){ reconnect(); return false; }
        std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(r, freeReplyObject);
        return r->type == REDIS_REPLY_INTEGER && r->integer >= 0;
    }

private:
    void connect() {
        struct timeval tv;
        tv.tv_sec = timeout_ms_ / 1000;
        tv.tv_usec = (timeout_ms_ % 1000) * 1000;
        ctx_ = redisConnectWithTimeout(host_.c_str(), port_, tv);
        if (!ctx_ || ctx_->err) {
            throw std::runtime_error("Failed to connect to Redis: " + std::string(ctx_ ? ctx_->errstr : "null ctx"));
        }
        redisReply* r = (redisReply*)redisCommand(ctx_, "SELECT %d", db_);
        if (!r) throw std::runtime_error("Redis SELECT failed");
        freeReplyObject(r);
    }

    void disconnect(){
        if(ctx_){ redisFree(ctx_); ctx_ = nullptr; }
    }
    void reconnect(){
        disconnect();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        connect();
    }
    void ensure(){
        if(!ctx_) reconnect();
    }

    std::string host_;
    int port_{6379};
    int db_{0};
    int timeout_ms_{2000};
    redisContext* ctx_{nullptr};
    std::mutex mu_;
};