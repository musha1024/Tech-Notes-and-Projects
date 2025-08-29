/**
 * @file redis_client.cpp
 * @brief Redis 客户端实现。
 */
#include "db/redis_client.h"
#include "app/logger.h"

RedisClient::RedisClient(const RedisConfig& cfg){
#ifdef USE_REDIS
    sw::redis::ConnectionOptions opts;
    opts.host = cfg.host; opts.port = cfg.port; opts.db = cfg.db;
    if(!cfg.password.empty()) opts.password = cfg.password;
    redis_ = std::make_unique<sw::redis::Redis>(opts);
    ttl_ = cfg.ttl_seconds;
    try{ redis_->ping(); LOGI("Redis ready: %s:%d", cfg.host.c_str(), cfg.port); }
    catch(const std::exception& e){ LOGE("Redis connect fail: %s", e.what()); throw; }
#else
    (void)cfg;
#endif
}

RedisClient::~RedisClient(){}

bool RedisClient::healthy() const{
#ifdef USE_REDIS
    try{ auto r = redis_->ping(); return !r.empty(); }catch(...){ return false; }
#else
    return false;
#endif
}

bool RedisClient::setex(const std::string& key, const std::string& val, int ttl_seconds){
#ifdef USE_REDIS
    try{
        redis_->set(key, val);
        if(ttl_seconds>0) redis_->expire(key, std::chrono::seconds(ttl_seconds));
        return true;
    }catch(...){ return false; }
#else
    (void)key; (void)val; (void)ttl_seconds; return false;
#endif
}

bool RedisClient::get(const std::string& key, std::string& out){
#ifdef USE_REDIS
    try{
        auto v = redis_->get(key);
        if(v){ out = *v; return true; }
        return false;
    }catch(...){ return false; }
#else
    (void)key; (void)out; return false;
#endif
}

bool RedisClient::del(const std::string& key){
#ifdef USE_REDIS
    try{ redis_->del(key); return true; }catch(...){ return false; }
#else
    (void)key; return false;
#endif
}
