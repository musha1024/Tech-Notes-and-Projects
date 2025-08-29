/**
 * @file mysql_pool.cpp
 * @brief MySQL 连接池实现。
 */
#include "db/mysql_pool.h"
#include "app/logger.h"

#include <jdbc/mysql_driver.h>
#include <jdbc/cppconn/driver.h>
#include <jdbc/cppconn/connection.h>
#include <jdbc/cppconn/statement.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/resultset.h>

static sql::Connection* create_one(sql::mysql::MySQL_Driver* driver, const MySqlConfig& cfg){
    std::unique_ptr<sql::Connection> con(driver->connect("tcp://" + cfg.host + ":" + std::to_string(cfg.port), cfg.user, cfg.password));
    con->setSchema(cfg.database);
    return con.release();
}

MySqlPool::MySqlPool(const MySqlConfig& cfg): cfg_(cfg){
    driver_ = sql::mysql::get_mysql_driver_instance();
    for(int i=0;i<cfg_.pool_size;i++){
        try{
            q_.push(create_one(driver_, cfg_));
            total_++;
        }catch(const std::exception& e){
            LOGE("MySQL connect fail: %s", e.what());
            throw;
        }
    }
    LOGI("MySQL pool ready: %d", total_);
}

MySqlPool::~MySqlPool(){
    while(!q_.empty()){ delete q_.front(); q_.pop(); }
}

MySqlPool::Guard MySqlPool::acquire(){
    std::unique_lock lk(mtx_);
    cv_.wait(lk, [&]{ return !q_.empty(); });
    auto* c = q_.front(); q_.pop();
    return Guard(this, c);
}

void MySqlPool::Guard::release(){
    if(pool_ && conn_){
        std::unique_lock lk(pool_->mtx_);
        pool_->q_.push(conn_);
        lk.unlock();
        pool_->cv_.notify_one();
    }
    pool_=nullptr; conn_=nullptr;
}
