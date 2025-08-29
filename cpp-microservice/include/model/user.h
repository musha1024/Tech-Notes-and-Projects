#pragma once
/**
 * @file user.h
 * @brief User 数据模型（含 JSON 序列化）。
 *
 * 语义：
 *  - id: 数据库主键
 *  - name: 用户名（示例）
 *  - age: 年龄（≥0）
 */
#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>

struct User {
    int64_t id{0};
    std::string name;
    int age{0};
};

inline void to_json(nlohmann::json& j, const User& u){
    j = nlohmann::json{{"id",u.id},{"name",u.name},{"age",u.age}};
}
inline void from_json(const nlohmann::json& j, User& u){
    j.at("id").get_to(u.id);
    j.at("name").get_to(u.name);
    j.at("age").get_to(u.age);
}
