#include <iostream>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "service/config.hpp"
#include "service/mysql_pool.hpp"
#include "service/handlers.hpp"

using namespace service;

int main(int argc, char** argv) {
    Config cfg = parse_args(argc, argv);
    std::cout << "MySQL Microservice (modular) at " << cfg.host << ":" << cfg.port << "\n"
              << "DB: " << cfg.mysql_user << "@" << cfg.mysql_host << ":" << cfg.mysql_port
              << " / " << cfg.mysql_db << "\n"
              << "Pool size: " << cfg.pool_size << std::endl;

    try {
        MySQLPool pool(cfg);
        httplib::Server svr;
        install_routes(svr, pool);
        std::cout << "Listening on http://" << cfg.host << ":" << cfg.port << " ..." << std::endl;
        if (!svr.listen(cfg.host.c_str(), cfg.port)) {
            std::cerr << "Failed to listen on " << cfg.host << ":" << cfg.port << std::endl;
            return 1;
        }
    } catch(const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
