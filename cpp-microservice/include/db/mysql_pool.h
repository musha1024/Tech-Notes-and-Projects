#pragma once
/**
 * @file mysql_pool.h
 * @brief 简易 MySQL 连接池（线程安全，RAII 归还）。
 *
 * 功能：
 *  - 初始化时创建固定数量连接
 *  - acquire() 获取连接句柄（阻塞等待）
 *  - Guard 析构即自动归还连接
 *
 * 输入：MySqlConfig（地址、账号、库名、池大小）
 * 输出：Guard 句柄（提供 Connection*）
 *
 * 可扩展：
 *  - 失败重连、连接健康检查
 *  - PreparedStatement 缓存
 *  - 动态伸缩池大小
 */
#include <memory>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <string>

namespace sql { class Connection; class PreparedStatement; class ResultSet; }
namespace sql { namespace mysql { class MySQL_Driver; } }

#include "app/config.h"

class MySqlPool {
public:
    explicit MySqlPool(const MySqlConfig& cfg);
    ~MySqlPool();

    /**
     * @brief RAII 连接句柄；析构自动归还。
     */
    class Guard {
    public:
        Guard(MySqlPool* pool, sql::Connection* conn): pool_(pool), conn_(conn) {}
        Guard(const Guard&) = delete; Guard& operator=(const Guard&) = delete;
        Guard(Guard&& o) noexcept: pool_(o.pool_), conn_(o.conn_){ o.pool_=nullptr; o.conn_=nullptr; }
        Guard& operator=(Guard&& o) noexcept { if(this!=&o){ release(); pool_=o.pool_; conn_=o.conn_; o.pool_=nullptr; o.conn_=nullptr; } return *this; }
        ~Guard(){ release(); }
        sql::Connection* operator->(){ return conn_; }
        sql::Connection* get(){ return conn_; }
    private:
        void release();
        MySqlPool* pool_{nullptr};
        sql::Connection* conn_{nullptr};
    };

    /**
     * @brief 获取一个连接（阻塞等待直到可用）。
     */
    Guard acquire();

private:
    sql::mysql::MySQL_Driver* driver_{nullptr};
    MySqlConfig cfg_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::queue<sql::Connection*> q_;
    int total_{0};
};
