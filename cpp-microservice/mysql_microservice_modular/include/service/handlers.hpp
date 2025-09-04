#pragma once
#include <string>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "service/config.hpp"
#include "service/mysql_pool.hpp"

namespace service {

void install_routes(httplib::Server& svr, MySQLPool& pool);

void json_error(httplib::Response& res, int status, const std::string& msg);

} // namespace service
