#include "service/handlers.hpp"
#include <stdexcept>

using nlohmann::json;

namespace service {

void json_error(httplib::Response& res, int status, const std::string& msg) {
    res.status = status;
    json j = { {"ok", false}, {"error", msg} };
    res.set_content(j.dump(), "application/json; charset=utf-8");
}

void install_routes(httplib::Server& svr, MySQLPool& pool) {
    // ping
    svr.Get("/ping", [](const httplib::Request&, httplib::Response& res){
        json j = { {"ok", true}, {"message", "pong"} };
        res.set_content(j.dump(), "application/json; charset=utf-8");
    });

    // init
    svr.Post("/admin/init", [&](const httplib::Request&, httplib::Response& res){
        auto c = pool.acquire();
        std::string err;
        if (!sql_init_tables(c->raw(), err)) {
            json_error(res, 500, "init failed: " + err);
            pool.release(std::move(c));
            return;
        }
        pool.release(std::move(c));
        json j = { {"ok", true}, {"message", "tables ready"} };
        res.set_content(j.dump(), "application/json; charset=utf-8");
    });

    // create
    svr.Post("/users", [&](const httplib::Request& req, httplib::Response& res){
        try {
            auto body = json::parse(req.body);
            std::string name = body.value("name", "");
            std::string email = body.value("email", "");
            if (name.empty() || email.empty()) { json_error(res, 400, "name/email required"); return; }
            if (name.size() > 128 || email.size() > 255) { json_error(res, 400, "name/email too long"); return; }
            auto c = pool.acquire();
            long long id = 0; std::string err;
            if (!sql_create_user(c->raw(), name, email, id, err)) {
                pool.release(std::move(c));
                json_error(res, 500, "create failed: " + err);
                return;
            }
            pool.release(std::move(c));
            json j = { {"ok", true}, {"id", id} };
            res.set_content(j.dump(), "application/json; charset=utf-8");
        } catch(const std::exception& e) {
            json_error(res, 400, std::string("invalid json: ")+e.what());
        }
    });

    // get one
    svr.Get(R"(/users/(\d+))", [&](const httplib::Request& req, httplib::Response& res){
        long long id = std::stoll(req.matches[1].str());
        auto c = pool.acquire();
        std::string err;
        auto row = sql_get_user(c->raw(), id, err);
        pool.release(std::move(c));
        if (!row.has_value()) {
            if (!err.empty()) json_error(res, 500, "query failed: " + err);
            else json_error(res, 404, "not found");
            return;
        }
        json j = {{"ok", true}, {"data", {{"id", row->id}, {"name", row->name}, {"email", row->email}, {"created_at", row->created_at}}}};
        res.set_content(j.dump(), "application/json; charset=utf-8");
    });

    // list
    svr.Get("/users", [&](const httplib::Request& req, httplib::Response& res){
        unsigned limit = 20, offset = 0;
        if (auto p = req.get_param_value("limit"); !p.empty()) { try { limit = (unsigned)std::stoul(p);} catch(...){} }
        if (auto p = req.get_param_value("offset"); !p.empty()) { try { offset = (unsigned)std::stoul(p);} catch(...){} }
        if (limit == 0) limit = 1;
        if (limit > 200) limit = 200;

        auto c = pool.acquire();
        std::vector<UserRow> rows; std::string err;
        if (!sql_list_users(c->raw(), limit, offset, rows, err)) {
            pool.release(std::move(c));
            json_error(res, 500, "list failed: " + err);
            return;
        }
        pool.release(std::move(c));
        json arr = json::array();
        for (auto& r : rows) arr.push_back({{"id", r.id}, {"name", r.name}, {"email", r.email}, {"created_at", r.created_at}});
        json j = { {"ok", true}, {"data", arr}, {"limit", limit}, {"offset", offset} };
        res.set_content(j.dump(), "application/json; charset=utf-8");
    });

    // update
    svr.Put(R"(/users/(\d+))", [&](const httplib::Request& req, httplib::Response& res){
        long long id = std::stoll(req.matches[1].str());
        try {
            auto body = json::parse(req.body);
            std::string name = body.value("name", "");
            std::string email = body.value("email", "");
            if (name.empty() || email.empty()) { json_error(res, 400, "name/email required"); return; }
            if (name.size() > 128 || email.size() > 255) { json_error(res, 400, "name/email too long"); return; }
            auto c = pool.acquire();
            bool not_found = false; std::string err;
            if (!sql_update_user(c->raw(), id, name, email, not_found, err)) {
                pool.release(std::move(c));
                json_error(res, 500, "update failed: " + err);
                return;
            }
            pool.release(std::move(c));
            if (not_found) { json_error(res, 404, "not found"); return; }
            json j = { {"ok", true} };
            res.set_content(j.dump(), "application/json; charset=utf-8");
        } catch(const std::exception& e) {
            json_error(res, 400, std::string("invalid json: ")+e.what());
        }
    });

    // delete
    svr.Delete(R"(/users/(\d+))", [&](const httplib::Request& req, httplib::Response& res){
        long long id = std::stoll(req.matches[1].str());
        auto c = pool.acquire();
        bool not_found = false; std::string err;
        if (!sql_delete_user(c->raw(), id, not_found, err)) {
            pool.release(std::move(c));
            json_error(res, 500, "delete failed: " + err);
            return;
        }
        pool.release(std::move(c));
        if (not_found) { json_error(res, 404, "not found"); return; }
        json j = { {"ok", true} };
        res.set_content(j.dump(), "application/json; charset=utf-8");
    });
}

} // namespace service
