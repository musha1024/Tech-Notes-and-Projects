/**
 * @file main.cpp
 * @brief 服务入口：初始化依赖、注册路由、启动 HTTP 服务。
 *
 * 路由：
 *  - GET  /health        健康检查
 *  - POST /user          插入用户（Body: {name, age}）→ 返回 {id}
 *  - GET  /user?id=123   查询用户 → 返回 {id, name, age}
 */
#include <httplib.h>
#include <nlohmann/json.hpp>
#include "app/config.h"
#include "app/logger.h"
#include "db/mysql_pool.h"
#include "db/redis_client.h"
#include "model/user_dao.h"
#include "util/strutil.h"

using json = nlohmann::json;

int main(int argc, char** argv){
    const char* cfg_path = (argc>1)? argv[1] : "./config.json";
    AppConfig cfg; if(!load_config(cfg_path, cfg)){ LOGE("load config fail: %s", cfg_path); return 1; }

    try{
        // 依赖初始化
        MySqlPool mysql(cfg.mysql);
        RedisClient* redis_ptr = nullptr;
#ifdef USE_REDIS
        RedisClient redis(cfg.redis); redis_ptr = &redis;
#endif
        UserDao dao(mysql, redis_ptr, cfg.redis.ttl_seconds);

        httplib::Server svr;
        svr.set_keep_alive_max_count(100);

        // 健康检查
        svr.Get("/health", [&](const httplib::Request&, httplib::Response& res){
            res.set_content("ok", "text/plain");
        });

        // 插入用户
        svr.Post("/user", [&](const httplib::Request& req, httplib::Response& res){
            try{
                auto j = json::parse(req.body);
                // 基本入参校验（可扩展：长度、字符集、范围等）
                std::string name = j.at("name").get<std::string>();
                int age = j.at("age").get<int>();
                if(name.empty() || name.size() > 64 || age < 0){
                    res.status = 400;
                    res.set_content(json({{"error","invalid input"}}).dump(), "application/json");
                    return;
                }
                auto id = dao.insert_user(name, age);
                if(id){ res.set_content(json({{"id", *id}}).dump(), "application/json"); }
                else { res.status=500; res.set_content(json({{"error","insert failed"}}).dump(), "application/json"); }
            }catch(const std::exception& e){
                res.status=400;
                res.set_content(json({{"error", e.what()}}).dump(), "application/json");
            }
        });

        // 查询用户：/user?id=123
        svr.Get("/user", [&](const httplib::Request& req, httplib::Response& res){
            auto id_it = req.params.find("id");
            if(id_it == req.params.end()){
                res.status=400; res.set_content(json({{"error","missing id"}}).dump(), "application/json"); return;
            }
            auto oid = to_ll(id_it->second);
            if(!oid){
                res.status=400; res.set_content(json({{"error","bad id"}}).dump(), "application/json"); return;
            }
            auto u = dao.get_user(*oid);
            if(u){ res.set_content(json(*u).dump(), "application/json"); }
            else { res.status=404; res.set_content(json({{"error","not found"}}).dump(), "application/json"); }
        });

        LOGI("listening on %s:%d", cfg.server.host.c_str(), cfg.server.port);
        if(!svr.bind_to_port(cfg.server.host.c_str(), cfg.server.port)){
            LOGE("bind failed on %s:%d", cfg.server.host.c_str(), cfg.server.port);
            return 2;
        }
        svr.listen_after_bind();
        return 0;
    }catch(const std::exception& e){
        LOGE("fatal: %s", e.what());
        return 3;
    }
}
