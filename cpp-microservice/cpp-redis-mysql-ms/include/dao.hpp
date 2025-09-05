#pragma once
#include "mysql_pool.hpp"
#include "utils.hpp"
#include <string>
#include <stdexcept>
#include <cstring>

#ifndef my_bool
#define my_bool bool
#endif

class DAO {
public:
    explicit DAO(MySQLPool& pool): pool_(pool){}

    bool create_table_if_not_exists(const std::string& table){
        if(!is_valid_table(table)) return false;
        auto h = pool_.acquire();
        MYSQL* c = h.get();
        std::string sql = "CREATE TABLE IF NOT EXISTS `" + table + "` ("
                          " k VARCHAR(255) PRIMARY KEY,"
                          " v TEXT,"
                          " updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP"
                          ") CHARACTER SET utf8mb4";
        return mysql_query(c, sql.c_str()) == 0;
    }

    bool get_value(const std::string& table, const std::string& key, std::string& out) {
        if(!is_valid_table(table)) return false;
        auto h = pool_.acquire();
        MYSQL* c = h.get();
        MYSQL_STMT* stmt = mysql_stmt_init(c);
        if(!stmt) throw std::runtime_error("mysql_stmt_init failed");
        std::string sql = "SELECT v FROM `" + table + "` WHERE k=?";
        if(mysql_stmt_prepare(stmt, sql.c_str(), (unsigned long)sql.size()) != 0){
            mysql_stmt_close(stmt); return false;
        }
        MYSQL_BIND bind[1]; std::memset(bind, 0, sizeof(bind));
        bind[0].buffer_type = MYSQL_TYPE_STRING;
        bind[0].buffer = (void*)key.data();
        bind[0].buffer_length = (unsigned long)key.size();
        if(mysql_stmt_bind_param(stmt, bind) != 0){
            mysql_stmt_close(stmt); return false;
        }
        if(mysql_stmt_execute(stmt) != 0){
            mysql_stmt_close(stmt); return false;
        }

        char buf[65536]; unsigned long len=0; my_bool is_null=0; my_bool err=0;
        MYSQL_BIND rbind[1]; std::memset(rbind, 0, sizeof(rbind));
        rbind[0].buffer_type = MYSQL_TYPE_STRING;
        rbind[0].buffer = buf;
        rbind[0].buffer_length = sizeof(buf);
        rbind[0].length = &len;
        rbind[0].is_null = &is_null;
        rbind[0].error = &err;

        if(mysql_stmt_bind_result(stmt, rbind) != 0){
            mysql_stmt_close(stmt); return false;
        }
        if(mysql_stmt_store_result(stmt) != 0){
            mysql_stmt_close(stmt); return false;
        }
        bool ok=false;
        if(mysql_stmt_fetch(stmt) == 0 && !is_null){
            out.assign(buf, buf + len);
            ok=true;
        }
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        return ok;
    }

    bool upsert_value(const std::string& table, const std::string& key, const std::string& val) {
        if(!is_valid_table(table)) return false;
        auto h = pool_.acquire();
        MYSQL* c = h.get();
        MYSQL_STMT* stmt = mysql_stmt_init(c);
        if(!stmt) throw std::runtime_error("mysql_stmt_init failed");
        std::string sql = "INSERT INTO `" + table + "`(k,v) VALUES(?,?) ON DUPLICATE KEY UPDATE v=VALUES(v)";
        if(mysql_stmt_prepare(stmt, sql.c_str(), (unsigned long)sql.size()) != 0){
            mysql_stmt_close(stmt); return false;
        }
        MYSQL_BIND bind[2]; std::memset(bind, 0, sizeof(bind));
        bind[0].buffer_type = MYSQL_TYPE_STRING;
        bind[0].buffer = (void*)key.data();
        bind[0].buffer_length = (unsigned long)key.size();

        bind[1].buffer_type = MYSQL_TYPE_STRING;
        bind[1].buffer = (void*)val.data();
        bind[1].buffer_length = (unsigned long)val.size();

        if(mysql_stmt_bind_param(stmt, bind) != 0){
            mysql_stmt_close(stmt); return false;
        }
        bool ok = (mysql_stmt_execute(stmt) == 0);
        mysql_stmt_close(stmt);
        return ok;
    }

    bool delete_key(const std::string& table, const std::string& key) {
        if(!is_valid_table(table)) return false;
        auto h = pool_.acquire();
        MYSQL* c = h.get();
        MYSQL_STMT* stmt = mysql_stmt_init(c);
        if(!stmt) throw std::runtime_error("mysql_stmt_init failed");
        std::string sql = "DELETE FROM `" + table + "` WHERE k=?";
        if(mysql_stmt_prepare(stmt, sql.c_str(), (unsigned long)sql.size()) != 0){
            mysql_stmt_close(stmt); return false;
        }
        MYSQL_BIND bind[1]; std::memset(bind, 0, sizeof(bind));
        bind[0].buffer_type = MYSQL_TYPE_STRING;
        bind[0].buffer = (void*)key.data();
        bind[0].buffer_length = (unsigned long)key.size();
        if(mysql_stmt_bind_param(stmt, bind) != 0){
            mysql_stmt_close(stmt); return false;
        }
        bool ok = (mysql_stmt_execute(stmt) == 0);
        mysql_stmt_close(stmt);
        return ok;
    }
private:
    MySQLPool& pool_;
};