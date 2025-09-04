#include "service/mysql_pool.hpp"
#include <iostream>
#include <stdexcept>
#include <cstring>

namespace service {

MySQLConn::MySQLConn() : conn_(nullptr) {}
MySQLConn::~MySQLConn() { close(); }

bool MySQLConn::open(const std::string& host, unsigned port,
          const std::string& user, const std::string& pwd,
          const std::string& db) {
    close();
    conn_ = mysql_init(nullptr);
    if (!conn_) return false;
    unsigned int timeout = 5;
    mysql_options(conn_, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    mysql_options(conn_, MYSQL_SET_CHARSET_NAME, "utf8mb4");
    if (!mysql_real_connect(conn_, host.c_str(), user.c_str(), pwd.c_str(),
                            db.c_str(), port, nullptr, CLIENT_MULTI_STATEMENTS)) {
        std::cerr << "MySQL connect failed: " << mysql_error(conn_) << std::endl;
        close();
        return false;
    }
    if (mysql_set_character_set(conn_, "utf8mb4") != 0) {
        std::cerr << "MySQL set charset failed: " << mysql_error(conn_) << std::endl;
        close();
        return false;
    }
    return true;
}

void MySQLConn::close() {
    if (conn_) { mysql_close(conn_); conn_ = nullptr; }
}

MYSQL* MySQLConn::raw() { return conn_; }

MySQLPool::MySQLPool(const Config& cfg) {
    for (unsigned i = 0; i < cfg.pool_size; ++i) {
        auto c = std::make_unique<MySQLConn>();
        if (!c->open(cfg.mysql_host, cfg.mysql_port, cfg.mysql_user, cfg.mysql_password, cfg.mysql_db)) {
            throw std::runtime_error("Failed to open MySQL connection for pool");
        }
        pool_.push(std::move(c));
    }
}

MySQLPool::~MySQLPool() {
    std::unique_lock<std::mutex> lk(mu_);
    while (!pool_.empty()) pool_.pop();
    closed_ = true;
}

std::unique_ptr<MySQLConn> MySQLPool::acquire() {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [&]{ return !pool_.empty() || closed_; });
    if (closed_) throw std::runtime_error("Pool closed");
    auto c = std::move(pool_.front());
    pool_.pop();
    return c;
}

void MySQLPool::release(std::unique_ptr<MySQLConn> c) {
    std::lock_guard<std::mutex> lk(mu_);
    pool_.push(std::move(c));
    cv_.notify_one();
}

// ---- SQL helpers (prepared statements) ----
bool sql_init_tables(MYSQL* conn, std::string& err) {
    const char* ddl =
        "CREATE TABLE IF NOT EXISTS users ("
        "  id BIGINT PRIMARY KEY AUTO_INCREMENT,"
        "  name VARCHAR(128) NOT NULL,"
        "  email VARCHAR(255) NOT NULL UNIQUE,"
        "  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";
    if (mysql_query(conn, ddl) != 0) {
        err = mysql_error(conn);
        return false;
    }
    return true;
}

bool sql_create_user(MYSQL* conn, const std::string& name, const std::string& email,
                     long long& new_id, std::string& err) {
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) { err = "mysql_stmt_init failed"; return false; }
    const char* sql = "INSERT INTO users(name, email) VALUES(?, ?)";
    if (mysql_stmt_prepare(stmt, sql, (unsigned long)std::strlen(sql)) != 0) {
        err = mysql_stmt_error(stmt); mysql_stmt_close(stmt); return false;
    }
    MYSQL_BIND bind[2] = {};
    unsigned long name_len = (unsigned long)name.size();
    unsigned long email_len = (unsigned long)email.size();
    bind[0].buffer_type = MYSQL_TYPE_STRING; bind[0].buffer = (void*)name.c_str(); bind[0].length = &name_len;
    bind[1].buffer_type = MYSQL_TYPE_STRING; bind[1].buffer = (void*)email.c_str(); bind[1].length = &email_len;

    if (mysql_stmt_bind_param(stmt, bind) != 0) { err = mysql_stmt_error(stmt); mysql_stmt_close(stmt); return false; }
    if (mysql_stmt_execute(stmt) != 0) { err = mysql_stmt_error(stmt); mysql_stmt_close(stmt); return false; }
    new_id = (long long)mysql_insert_id(conn);
    mysql_stmt_close(stmt);
    return true;
}

std::optional<UserRow> sql_get_user(MYSQL* conn, long long id, std::string& err) {
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) { err = "mysql_stmt_init failed"; return std::nullopt; }
    const char* sql = "SELECT id, name, email, DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') FROM users WHERE id = ?";
    if (mysql_stmt_prepare(stmt, sql, (unsigned long)std::strlen(sql)) != 0) {
        err = mysql_stmt_error(stmt); mysql_stmt_close(stmt); return std::nullopt;
    }
    MYSQL_BIND bind_param[1] = {};
    bind_param[0].buffer_type = MYSQL_TYPE_LONGLONG; bind_param[0].buffer = (void*)&id;
    if (mysql_stmt_bind_param(stmt, bind_param) != 0) { err = mysql_stmt_error(stmt); mysql_stmt_close(stmt); return std::nullopt; }
    if (mysql_stmt_execute(stmt) != 0) { err = mysql_stmt_error(stmt); mysql_stmt_close(stmt); return std::nullopt; }

    long long r_id = 0;
    char r_name[129] = {0}; unsigned long r_name_len = 0;
    char r_email[256] = {0}; unsigned long r_email_len = 0;
    char r_created[20] = {0}; unsigned long r_created_len = 0;

    MYSQL_BIND bind_res[4] = {};
    bind_res[0].buffer_type = MYSQL_TYPE_LONGLONG; bind_res[0].buffer = (void*)&r_id;
    bind_res[1].buffer_type = MYSQL_TYPE_STRING;   bind_res[1].buffer = (void*)r_name;  bind_res[1].buffer_length = sizeof(r_name)-1;  bind_res[1].length = &r_name_len;
    bind_res[2].buffer_type = MYSQL_TYPE_STRING;   bind_res[2].buffer = (void*)r_email; bind_res[2].buffer_length = sizeof(r_email)-1; bind_res[2].length = &r_email_len;
    bind_res[3].buffer_type = MYSQL_TYPE_STRING;   bind_res[3].buffer = (void*)r_created; bind_res[3].buffer_length = sizeof(r_created)-1; bind_res[3].length = &r_created_len;

    if (mysql_stmt_bind_result(stmt, bind_res) != 0) { err = mysql_stmt_error(stmt); mysql_stmt_close(stmt); return std::nullopt; }
    if (mysql_stmt_store_result(stmt) != 0) { err = mysql_stmt_error(stmt); mysql_stmt_close(stmt); return std::nullopt; }

    std::optional<UserRow> out;
    int rc = mysql_stmt_fetch(stmt);
    if (rc == 0) {
        UserRow row;
        row.id = r_id;
        row.name.assign(r_name, r_name_len);
        row.email.assign(r_email, r_email_len);
        row.created_at.assign(r_created, r_created_len);
        out = row;
    } else if (rc == MYSQL_NO_DATA) {
        out = std::nullopt;
    } else {
        err = mysql_stmt_error(stmt);
        out = std::nullopt;
    }
    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return out;
}

bool sql_update_user(MYSQL* conn, long long id, const std::string& name, const std::string& email,
                     bool& not_found, std::string& err) {
    not_found = false;
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) { err = "mysql_stmt_init failed"; return false; }
    const char* sql = "UPDATE users SET name = ?, email = ? WHERE id = ?";
    if (mysql_stmt_prepare(stmt, sql, (unsigned long)std::strlen(sql)) != 0) { err = mysql_stmt_error(stmt); mysql_stmt_close(stmt); return false; }
    MYSQL_BIND bind[3] = {};
    unsigned long name_len = (unsigned long)name.size();
    unsigned long email_len = (unsigned long)email.size();
    bind[0].buffer_type = MYSQL_TYPE_STRING; bind[0].buffer = (void*)name.c_str();  bind[0].length = &name_len;
    bind[1].buffer_type = MYSQL_TYPE_STRING; bind[1].buffer = (void*)email.c_str(); bind[1].length = &email_len;
    bind[2].buffer_type = MYSQL_TYPE_LONGLONG; bind[2].buffer = (void*)&id;

    if (mysql_stmt_bind_param(stmt, bind) != 0) { err = mysql_stmt_error(stmt); mysql_stmt_close(stmt); return false; }
    if (mysql_stmt_execute(stmt) != 0) { err = mysql_stmt_error(stmt); mysql_stmt_close(stmt); return false; }
    my_ulonglong affected = mysql_stmt_affected_rows(stmt);
    if (affected == 0) not_found = true;
    mysql_stmt_close(stmt);
    return true;
}

bool sql_delete_user(MYSQL* conn, long long id, bool& not_found, std::string& err) {
    not_found = false;
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) { err = "mysql_stmt_init failed"; return false; }
    const char* sql = "DELETE FROM users WHERE id = ?";
    if (mysql_stmt_prepare(stmt, sql, (unsigned long)std::strlen(sql)) != 0) { err = mysql_stmt_error(stmt); mysql_stmt_close(stmt); return false; }
    MYSQL_BIND bind[1] = {};
    bind[0].buffer_type = MYSQL_TYPE_LONGLONG; bind[0].buffer = (void*)&id;
    if (mysql_stmt_bind_param(stmt, bind) != 0) { err = mysql_stmt_error(stmt); mysql_stmt_close(stmt); return false; }
    if (mysql_stmt_execute(stmt) != 0) { err = mysql_stmt_error(stmt); mysql_stmt_close(stmt); return false; }
    my_ulonglong affected = mysql_stmt_affected_rows(stmt);
    if (affected == 0) not_found = true;
    mysql_stmt_close(stmt);
    return true;
}

bool sql_list_users(MYSQL* conn, unsigned limit, unsigned offset,
                    std::vector<UserRow>& out, std::string& err) {
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) { err = "mysql_stmt_init failed"; return false; }
    const char* sql = "SELECT id, name, email, DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') FROM users ORDER BY id LIMIT ? OFFSET ?";
    if (mysql_stmt_prepare(stmt, sql, (unsigned long)std::strlen(sql)) != 0) { err = mysql_stmt_error(stmt); mysql_stmt_close(stmt); return false; }

    MYSQL_BIND bind_param[2] = {};
    unsigned long l = limit, o = offset;
    bind_param[0].buffer_type = MYSQL_TYPE_LONG; bind_param[0].buffer = (void*)&l;
    bind_param[1].buffer_type = MYSQL_TYPE_LONG; bind_param[1].buffer = (void*)&o;
    if (mysql_stmt_bind_param(stmt, bind_param) != 0) { err = mysql_stmt_error(stmt); mysql_stmt_close(stmt); return false; }
    if (mysql_stmt_execute(stmt) != 0) { err = mysql_stmt_error(stmt); mysql_stmt_close(stmt); return false; }

    long long r_id = 0;
    char r_name[129] = {0}; unsigned long r_name_len = 0;
    char r_email[256] = {0}; unsigned long r_email_len = 0;
    char r_created[20] = {0}; unsigned long r_created_len = 0;

    MYSQL_BIND bind_res[4] = {};
    bind_res[0].buffer_type = MYSQL_TYPE_LONGLONG; bind_res[0].buffer = (void*)&r_id;
    bind_res[1].buffer_type = MYSQL_TYPE_STRING;   bind_res[1].buffer = (void*)r_name;  bind_res[1].buffer_length = sizeof(r_name)-1;  bind_res[1].length = &r_name_len;
    bind_res[2].buffer_type = MYSQL_TYPE_STRING;   bind_res[2].buffer = (void*)r_email; bind_res[2].buffer_length = sizeof(r_email)-1; bind_res[2].length = &r_email_len;
    bind_res[3].buffer_type = MYSQL_TYPE_STRING;   bind_res[3].buffer = (void*)r_created; bind_res[3].buffer_length = sizeof(r_created)-1; bind_res[3].length = &r_created_len;

    if (mysql_stmt_bind_result(stmt, bind_res) != 0) { err = mysql_stmt_error(stmt); mysql_stmt_close(stmt); return false; }
    if (mysql_stmt_store_result(stmt) != 0) { err = mysql_stmt_error(stmt); mysql_stmt_close(stmt); return false; }
    out.clear();
    while (true) {
        int rc = mysql_stmt_fetch(stmt);
        if (rc == 0) {
            UserRow row;
            row.id = r_id;
            row.name.assign(r_name, r_name_len);
            row.email.assign(r_email, r_email_len);
            row.created_at.assign(r_created, r_created_len);
            out.push_back(std::move(row));
        } else if (rc == MYSQL_NO_DATA) {
            break;
        } else {
            err = mysql_stmt_error(stmt);
            mysql_stmt_free_result(stmt);
            mysql_stmt_close(stmt);
            return false;
        }
    }
    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return true;
}

} // namespace service
