#pragma once
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include <mysql.h>

#include "service/config.hpp"

namespace service {

class MySQLConn {
public:
    MySQLConn();
    ~MySQLConn();
    bool open(const std::string& host, unsigned port,
              const std::string& user, const std::string& pwd,
              const std::string& db);
    void close();
    MYSQL* raw();
private:
    MYSQL* conn_;
};

class MySQLPool {
public:
    explicit MySQLPool(const Config& cfg);
    ~MySQLPool();
    std::unique_ptr<MySQLConn> acquire();
    void release(std::unique_ptr<MySQLConn> c);
private:
    std::mutex mu_;
    std::condition_variable cv_;
    std::queue<std::unique_ptr<MySQLConn>> pool_;
    bool closed_{false};
};

struct UserRow {
    long long id;
    std::string name;
    std::string email;
    std::string created_at;
};

bool sql_init_tables(MYSQL* conn, std::string& err);
bool sql_create_user(MYSQL* conn, const std::string& name, const std::string& email,
                     long long& new_id, std::string& err);
std::optional<UserRow> sql_get_user(MYSQL* conn, long long id, std::string& err);
bool sql_update_user(MYSQL* conn, long long id, const std::string& name, const std::string& email,
                     bool& not_found, std::string& err);
bool sql_delete_user(MYSQL* conn, long long id, bool& not_found, std::string& err);
bool sql_list_users(MYSQL* conn, unsigned limit, unsigned offset,
                    std::vector<UserRow>& out, std::string& err);

} // namespace service
