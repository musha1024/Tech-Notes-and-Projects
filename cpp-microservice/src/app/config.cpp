/**
 * @file config.cpp
 * @brief JSON 配置加载实现。
 */
#include "app/config.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <type_traits>

using json = nlohmann::json;

template<typename T>
static void assign_if(json& j, const char* k, T& dst){
    if(j.contains(k)) dst = j[k].get<std::decay_t<T>>();
}

bool load_config(const std::string& path, AppConfig& out){
    std::ifstream ifs(path);
    if(!ifs.is_open()) return false;
    json j; ifs >> j;
    if(j.contains("server")){
        auto s = j["server"];
        assign_if(s, "host", out.server.host);
        assign_if(s, "port", out.server.port);
    }
    if(j.contains("mysql")){
        auto m = j["mysql"];
        assign_if(m, "host", out.mysql.host);
        assign_if(m, "port", out.mysql.port);
        assign_if(m, "user", out.mysql.user);
        assign_if(m, "password", out.mysql.password);
        assign_if(m, "database", out.mysql.database);
        assign_if(m, "pool_size", out.mysql.pool_size);
    }
    if(j.contains("redis")){
        auto r = j["redis"];
        assign_if(r, "host", out.redis.host);
        assign_if(r, "port", out.redis.port);
        assign_if(r, "password", out.redis.password);
        assign_if(r, "db", out.redis.db);
        assign_if(r, "ttl_seconds", out.redis.ttl_seconds);
    }
    return true;
}
