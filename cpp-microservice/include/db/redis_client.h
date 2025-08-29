#pragma once
/**
 * @file redis_client.h
 * @brief Redis 客户端封装（可选编译）。
 *
 * 功能：get/setex/del；用于缓存旁路策略。
 * 可扩展：
 *  - 连接池 / Pipeline / Lua 脚本
 *  - 热点 Key 互斥锁（SETNX）
 */
#include "app/config.h"
#include <memory>
#include <string>

#ifdef USE_REDIS
#include <sw/redis++/redis++.h>
#endif

class RedisClient {
public:
    explicit RedisClient(const RedisConfig& cfg);
    ~RedisClient();

    bool healthy() const;
    bool setex(const std::string& key, const std::string& val, int ttl_seconds);
    bool get(const std::string& key, std::string& out);
    bool del(const std::string& key);

private:
#ifdef USE_REDIS
    std::unique_ptr<sw::redis::Redis> redis_;
    int ttl_{300};
#endif
};
