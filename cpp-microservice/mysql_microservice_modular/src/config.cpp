#include "service/config.hpp"
#include <cstring>
#include <string>

namespace service {

std::optional<std::string> get_arg(int argc, char** argv, const char* key) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], key) == 0 && i + 1 < argc) {
            return std::string(argv[i + 1]);
        }
    }
    return std::nullopt;
}

Config parse_args(int argc, char** argv) {
    Config cfg;
    if (auto v = get_arg(argc, argv, "--host")) cfg.host = *v;
    if (auto v = get_arg(argc, argv, "--port")) cfg.port = std::stoi(*v);
    if (auto v = get_arg(argc, argv, "--mysql-host")) cfg.mysql_host = *v;
    if (auto v = get_arg(argc, argv, "--mysql-port")) cfg.mysql_port = (unsigned)std::stoul(*v);
    if (auto v = get_arg(argc, argv, "--mysql-user")) cfg.mysql_user = *v;
    if (auto v = get_arg(argc, argv, "--mysql-password")) cfg.mysql_password = *v;
    if (auto v = get_arg(argc, argv, "--mysql-db")) cfg.mysql_db = *v;
    if (auto v = get_arg(argc, argv, "--pool-size")) cfg.pool_size = (unsigned)std::stoul(*v);
    return cfg;
}

} // namespace service
