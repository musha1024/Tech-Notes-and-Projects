/**
 * @file user_dao.cpp
 * @brief User DAO 实现（PreparedStatement + Cache-Aside）。
 */
#include "model/user_dao.h"
#include "app/logger.h"
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/resultset.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

std::optional<int64_t> UserDao::insert_user(const std::string& name, int age){
    try{
        auto guard = mysql_.acquire();
        // ✅ 预编译防注入
        std::unique_ptr<sql::PreparedStatement> stmt(guard->prepareStatement(
            "INSERT INTO users(name, age) VALUES(?, ?)"));
        stmt->setString(1, name);
        stmt->setInt(2, age);
        stmt->executeUpdate();

        // 获取自增 id
        std::unique_ptr<sql::Statement> st(guard->createStatement());
        std::unique_ptr<sql::ResultSet> rs(st->executeQuery("SELECT LAST_INSERT_ID()"));
        if(rs->next()){
            int64_t id = rs->getInt64(1);
            if(redis_){
                User u{ id, name, age };
                std::string val = json(u).dump();
                redis_->setex(cache_key(id), val, ttl_);
            }
            return id;
        }
        return std::nullopt;
    }catch(const std::exception& e){
        LOGE("insert_user error: %s", e.what());
        return std::nullopt;
    }
}

std::optional<User> UserDao::get_user(int64_t id){
    // 先查缓存
    if(redis_){
        std::string val;
        if(redis_->get(cache_key(id), val)){
            try{
                auto j = json::parse(val);
                User u = j.get<User>();
                return u;
            }catch(...){
                // 解析失败则忽略该缓存
            }
        }
    }

    // 回源数据库
    try{
        auto guard = mysql_.acquire();
        std::unique_ptr<sql::PreparedStatement> stmt(guard->prepareStatement(
            "SELECT id, name, age FROM users WHERE id=?"));
        stmt->setInt64(1, id);
        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
        if(rs->next()){
            User u; u.id = rs->getInt64(1); u.name = rs->getString(2); u.age = rs->getInt(3);
            if(redis_){
                std::string val = json(u).dump();
                redis_->setex(cache_key(id), val, ttl_);
            }
            return u;
        }
        return std::nullopt;
    }catch(const std::exception& e){
        LOGE("get_user error: %s", e.what());
        return std::nullopt;
    }
}
