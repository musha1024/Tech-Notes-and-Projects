#pragma once
/**
 * @file config.h
 * @brief 应用配置模型与加载接口。
 *
 * 模块职责：
 *  - 定义 Server / MySQL / Redis 配置结构体
 *  - 暴露 load_config(path, out) 从 JSON 载入配置
 *
 * 可扩展：
 *  - 新增 Kafka/Etcd 等配置，只需在 AppConfig 中添加，并在 config.cpp 中解析。
 */
#include <string>
#include <cstdint>

struct ServerConfig { std::string host = "0.0.0.0"; int port = 8080; };

struct MySqlConfig {
    std::string host = "127.0.0.1";
    int port = 3306;
    std::string user = "root";
    std::string password = "root";
    std::string database = "demo";
    int pool_size = 5;        // 连接池尺寸，压测后再行确定
};

struct RedisConfig {
    std::string host = "127.0.0.1";
    int port = 6379;
    std::string password = "";
    int db = 0;
    int ttl_seconds = 300;    // 默认缓存过期时间（秒）
};

struct AppConfig {
    ServerConfig server;
    MySqlConfig mysql;
    RedisConfig redis;
};

/**
 * @brief 从 JSON 文件加载配置。
 * @param path 配置文件路径
 * @param out  输出对象
 * @return true 加载成功；false 读取/解析失败
 */
bool load_config(const std::string& path, AppConfig& out);
