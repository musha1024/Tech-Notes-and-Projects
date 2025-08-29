#pragma once
/**
 * @file user_dao.h
 * @brief User DAO（含缓存）
 *
 * 职责：对 User 表执行 CRUD（此处演示插入/查询），并按需读写缓存。
 *
 * 一致性策略：Cache-Aside
 *  - get_user(id): 先查 Redis → miss 则查 MySQL，命中后回填缓存
 *  - insert_user: 写 MySQL 成功后，写入/刷新缓存
 */
#include <optional>
#include <string>
#include "db/mysql_pool.h"
#include "db/redis_client.h"
#include "model/user.h"

class UserDao {
public:
    UserDao(MySqlPool& mysql, RedisClient* redis, int ttl_seconds)
      : mysql_(mysql), redis_(redis), ttl_(ttl_seconds) {}

    /**
     * @brief 插入用户
     * @param name 用户名（非空，<=64）
     * @param age  年龄（>=0）
     * @return 新记录 id；失败返回 nullopt
     */
    std::optional<int64_t> insert_user(const std::string& name, int age);

    /**
     * @brief 查询用户（优先缓存）
     * @param id 用户 id
     * @return User；未找到返回 nullopt
     */
    std::optional<User> get_user(int64_t id);

private:
    std::string cache_key(int64_t id) const { return "user:" + std::to_string(id); }

    MySqlPool& mysql_;
    RedisClient* redis_{nullptr};
    int ttl_{300};
};
