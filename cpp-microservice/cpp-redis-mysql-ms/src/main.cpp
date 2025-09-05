#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <thread>
#include <sstream>
#include <iostream>
#include <map>
#include <algorithm>

#include "utils.hpp"
#include "redis_client.hpp"
#include "mysql_pool.hpp"
#include "dao.hpp"

struct Config {
    int port = 8081;
    std::string redis_host = "127.0.0.1";
    int redis_port = 6379;
    int redis_db = 0;
    std::string mysql_host = "127.0.0.1";
    int mysql_port = 3306;
    std::string mysql_user = "root";
    std::string mysql_pass = "";
    std::string mysql_db   = "test";
    int mysql_pool_size = 4;
    int default_ttl = 300; // seconds
};

static void print_help(){
    std::cout << "Usage: ./redis_mysql_ms "
              << "--port 8081 "
              << "--redis-host 127.0.0.1 --redis-port 6379 --redis-db 0 "
              << "--mysql-host 127.0.0.1 --mysql-port 3306 --mysql-user root --mysql-password '' --mysql-db test "
              << "--mysql-pool-size 4 --default-ttl 300\n";
}

static bool parse_args(int argc, char** argv, Config& cfg){
    for(int i=1;i<argc;i++){
        std::string a = argv[i];
        if(a=="--help"){ print_help(); return false; }
        else if(a=="--port" && i+1<argc){ cfg.port = std::stoi(argv[++i]); }
        else if(a=="--redis-host" && i+1<argc){ cfg.redis_host = argv[++i]; }
        else if(a=="--redis-port" && i+1<argc){ cfg.redis_port = std::stoi(argv[++i]); }
        else if(a=="--redis-db" && i+1<argc){ cfg.redis_db = std::stoi(argv[++i]); }
        else if(a=="--mysql-host" && i+1<argc){ cfg.mysql_host = argv[++i]; }
        else if(a=="--mysql-port" && i+1<argc){ cfg.mysql_port = std::stoi(argv[++i]); }
        else if(a=="--mysql-user" && i+1<argc){ cfg.mysql_user = argv[++i]; }
        else if(a=="--mysql-password" && i+1<argc){ cfg.mysql_pass = argv[++i]; }
        else if(a=="--mysql-db" && i+1<argc){ cfg.mysql_db = argv[++i]; }
        else if(a=="--mysql-pool-size" && i+1<argc){ cfg.mysql_pool_size = std::stoi(argv[++i]); }
        else if(a=="--default-ttl" && i+1<argc){ cfg.default_ttl = std::stoi(argv[++i]); }
    }
    return true;
}

static std::map<std::string,std::string> parse_query(const std::string& uri){
    std::map<std::string,std::string> q;
    auto pos = uri.find('?');
    if(pos == std::string::npos) return q;
    std::string qs = uri.substr(pos+1);
    std::istringstream ss(qs);
    std::string item;
    while(std::getline(ss, item, '&')){
        auto eq = item.find('=');
        if(eq == std::string::npos) continue;
        auto k = url_decode(item.substr(0,eq));
        auto v = url_decode(item.substr(eq+1));
        q[k] = v;
    }
    return q;
}

static void send_response(int client_fd, int status, const std::string& body, const std::string& content_type="application/json"){
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status << " OK\r\n";
    oss << "Content-Type: " << content_type << "\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Connection: close\r\n\r\n";
    oss << body;
    auto s = oss.str();
    send(client_fd, s.data(), s.size(), 0);
}

static std::string pick_table(const std::map<std::string,std::string>& q){
    auto it = q.find("table");
    if(it != q.end() && is_valid_table(it->second)) return it->second;
    return "kv_store";
}

int main(int argc, char** argv){
    Config cfg;
    if(!parse_args(argc, argv, cfg)) return 0;

    try {
        MySQLPool mysql_pool(cfg.mysql_host, cfg.mysql_port, cfg.mysql_user, cfg.mysql_pass, cfg.mysql_db, cfg.mysql_pool_size);
        DAO dao(mysql_pool);

        // default table exists
        {
            auto h = mysql_pool.acquire();
            auto* c = h.get();
            const char* sql =
                "CREATE TABLE IF NOT EXISTS kv_store ("
                " k VARCHAR(255) PRIMARY KEY,"
                " v TEXT,"
                " updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP"
                ") CHARACTER SET utf8mb4";
            mysql_query(c, sql);
        }

        RedisClient rds(cfg.redis_host, cfg.redis_port, cfg.redis_db);

        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1; setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_addr.s_addr = INADDR_ANY; addr.sin_port = htons(cfg.port);
        if(bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0){ perror("bind"); return 1; }
        if(listen(server_fd, 64) < 0){ perror("listen"); return 1; }
        std::cout << "[OK] Listening on 0.0.0.0:" << cfg.port << " ...\n";

        while(true){
            int client = accept(server_fd, nullptr, nullptr);
            if(client < 0){ perror("accept"); continue; }

            // ---- read request ----
            std::string req;
            char buf[4096];
            ssize_t n;
            while((n = recv(client, buf, sizeof(buf), 0)) > 0){
                req.append(buf, buf+n);
                if(req.find("\r\n\r\n") != std::string::npos) break;
            }
            if(req.empty()){ close(client); continue; }

            auto line_end = req.find("\r\n");
            std::string start = req.substr(0, line_end);
            std::istringstream ss(start);
            std::string method, uri, httpver;
            ss >> method >> uri >> httpver;

            size_t header_end = req.find("\r\n\r\n");
            std::string headers = req.substr(0, header_end+4);
            size_t clen = 0;
            auto clpos = headers.find("Content-Length:");
            if(clpos != std::string::npos){
                clpos += 15;
                while(clpos<headers.size() && (headers[clpos]==' ')) clpos++;
                size_t end = headers.find("\r\n", clpos);
                clen = std::stoul(headers.substr(clpos, end-clpos));
            }
            std::string body;
            if(clen > 0){
                size_t have = req.size() - (header_end+4);
                if(have >= clen){
                    body = req.substr(header_end+4, clen);
                } else {
                    body = req.substr(header_end+4);
                    size_t remain = clen - have;
                    while(remain > 0 && (n = recv(client, buf, std::min(sizeof(buf),(size_t)remain), 0)) > 0){
                        body.append(buf, buf+n);
                        remain -= n;
                    }
                }
            }

            auto q = parse_query(uri);
            std::string table = pick_table(q);

            // ---- routes ----
            if(method == "GET" && uri.rfind("/health", 0) == 0){
                send_response(client, 200, "{\"status\":\"ok\"}");
            }
            else if(method == "POST" && uri.rfind("/table/create", 0) == 0){
                std::string name;
                bool ok = json_get_string(body, "name", name) && is_valid_table(name);
                if(!ok){
                    send_response(client, 400, "{\"error\":\"invalid table name\"}");
                } else {
                    bool ok2 = dao.create_table_if_not_exists(name);
                    std::string resp = std::string("{\"ok\":") + (ok2?"true":"false") + ",\"table\":\""+json_escape(name)+"\"}";
                    send_response(client, 200, resp);
                }
            }
            else if(method == "GET" && uri.rfind("/get", 0) == 0){
                auto it = q.find("key");
                if(it == q.end() || !is_valid_key(it->second) || !is_valid_table(table)){
                    send_response(client, 400, "{\"error\":\"invalid key/table\"}");
                } else {
                    std::string key = it->second;
                    std::string val;
                    if(rds.get(key, val)){
                        std::string resp = std::string("{\"source\":\"redis\",\"table\":\"")+json_escape(table)+"\",\"key\":\""+json_escape(key)+"\",\"value\":\""+json_escape(val)+"\"}";
                        send_response(client, 200, resp);
                    } else {
                        if(dao.get_value(table, key, val)){
                            rds.setex(key, val, cfg.default_ttl);
                            std::string resp = std::string("{\"source\":\"mysql\",\"table\":\"")+json_escape(table)+"\",\"key\":\""+json_escape(key)+"\",\"value\":\""+json_escape(val)+"\"}";
                            send_response(client, 200, resp);
                        } else {
                            send_response(client, 404, "{\"error\":\"not found\"}");
                        }
                    }
                }
            }
            else if(method == "POST" && uri.rfind("/set", 0) == 0){
                std::string key, value; int ttl=cfg.default_ttl;
                bool ok1 = json_get_string(body, "key", key);
                bool ok2 = json_get_string(body, "value", value);
                json_get_int(body, "ttl", ttl);
                if(!ok1 || !ok2 || !is_valid_key(key) || !is_valid_table(table)){
                    send_response(client, 400, "{\"error\":\"invalid body/table\"}");
                } else {
                    bool ok = dao.upsert_value(table, key, value);
                    if(ok) rds.setex(key, value, ttl);
                    std::string resp = std::string("{\"ok\":") + (ok?"true":"false") + ",\"table\":\""+json_escape(table)+"\",\"policy\":\"write-through\"}";
                    send_response(client, 200, resp);
                }
            }
            else if(method == "POST" && uri.rfind("/update", 0) == 0){
                std::string key, value; int ttl=cfg.default_ttl;
                bool ok1 = json_get_string(body, "key", key);
                bool ok2 = json_get_string(body, "value", value);
                json_get_int(body, "ttl", ttl);
                if(!ok1 || !ok2 || !is_valid_key(key) || !is_valid_table(table)){
                    send_response(client, 400, "{\"error\":\"invalid body/table\"}");
                } else {
                    bool ok = dao.upsert_value(table, key, value);
                    if(ok){
                        rds.setex(key, value, ttl);
                        std::string resp = std::string("{\"ok\":true,\"table\":\"")+json_escape(table)+"\",\"policy\":\"write-through\",\"cache\":\"refreshed\"}";
                        send_response(client, 200, resp);
                    } else {
                        send_response(client, 500, "{\"ok\":false}");
                    }
                }
            }
            else if(method == "DELETE" && uri.rfind("/del", 0) == 0){
                auto it = q.find("key");
                if(it == q.end() || !is_valid_key(it->second) || !is_valid_table(table)){
                    send_response(client, 400, "{\"error\":\"invalid key/table\"}");
                } else {
                    std::string key = it->second;
                    bool ok = dao.delete_key(table, key);
                    rds.del(key);
                    std::string resp = std::string("{\"ok\":") + (ok?"true":"false") + ",\"table\":\""+json_escape(table)+"\"}";
                    send_response(client, 200, resp);
                }
            }
            else {
                send_response(client, 404, "{\"error\":\"unknown route\"}");
            }

            close(client);
        }
    } catch(const std::exception& e){
        std::cerr << "[FATAL] " << e.what() << std::endl;
        return 1;
    }
    return 0;
}