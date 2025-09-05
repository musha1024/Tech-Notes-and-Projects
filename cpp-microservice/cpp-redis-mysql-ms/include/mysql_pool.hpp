#pragma once
#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <stdexcept>

class MySQLPool;

class MySQLHandle {
public:
    MySQLHandle(MYSQL* c, MySQLPool* p): conn_(c), pool_(p) {}
    ~MySQLHandle();
    MYSQL* get() const { return conn_; }
    MySQLHandle(const MySQLHandle&) = delete;
    MySQLHandle& operator=(const MySQLHandle&) = delete;
    MySQLHandle(MySQLHandle&& other) noexcept {
        conn_ = other.conn_; pool_ = other.pool_; other.conn_ = nullptr; other.pool_ = nullptr;
    }
    MySQLHandle& operator=(MySQLHandle&& other) noexcept {
        if(this != &other){
            conn_ = other.conn_; pool_ = other.pool_; other.conn_ = nullptr; other.pool_ = nullptr;
        }
        return *this;
    }
private:
    MYSQL* conn_{nullptr};
    MySQLPool* pool_{nullptr};
};

class MySQLPool {
public:
    MySQLPool(const std::string& host, int port, const std::string& user,
              const std::string& pass, const std::string& db, int pool_size=4) :
              host_(host), port_(port), user_(user), pass_(pass), db_(db) {
        for(int i=0;i<pool_size;i++){
            MYSQL* c = mysql_init(nullptr);
            if(!c) throw std::runtime_error("mysql_init failed");
            if(!mysql_real_connect(c, host_.c_str(), user_.c_str(), pass_.c_str(),
                                   db_.c_str(), port_, nullptr, 0)){
                throw std::runtime_error(std::string("MySQL connect failed: ")+mysql_error(c));
            }
            mysql_set_character_set(c, "utf8mb4");
            conns_.push(c);
        }
    }
    ~MySQLPool(){
        std::lock_guard<std::mutex> lk(mu_);
        while(!conns_.empty()){ mysql_close(conns_.front()); conns_.pop(); }
    }

    MySQLHandle acquire(){
        std::unique_lock<std::mutex> lk(mu_);
        cv_.wait(lk, [&]{ return !conns_.empty(); });
        MYSQL* c = conns_.front(); conns_.pop();
        return MySQLHandle(c, this);
    }

private:
    friend class MySQLHandle;
    void release(MYSQL* c){
        std::lock_guard<std::mutex> lk(mu_);
        conns_.push(c); cv_.notify_one();
    }

    std::string host_; int port_;
    std::string user_, pass_, db_;
    std::queue<MYSQL*> conns_;
    std::mutex mu_; std::condition_variable cv_;
};

inline MySQLHandle::~MySQLHandle(){
    if(conn_ && pool_) pool_->release(conn_);
}