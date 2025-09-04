#pragma once
#include <string>
#include <optional>

namespace service {

struct Config {
    std::string host = "0.0.0.0";
    int port = 8081;

    std::string mysql_host = "127.0.0.1";
    unsigned mysql_port = 3306;
    std::string mysql_user = "root";
    std::string mysql_password = "";
    std::string mysql_db = "test";
    unsigned pool_size = 8;
};

std::optional<std::string> get_arg(int argc, char** argv, const char* key);
Config parse_args(int argc, char** argv);

} // namespace service
