#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include <vector>
#include <string>

#include <json/json.h>
#include <httplib.h>
#include <websocketpp/server.hpp>

#include "Authorizer.hpp"
#include "ChatHistory.hpp"

typedef websocketpp::log::basic<websocketpp::concurrency::basic, websocketpp::log::elevel> basic_elog;

class HttpServer {
    Authorizer& auth = Authorizer::get_instance();
    ChatHistory& chat_history = ChatHistory::get_instance();

    httplib::Server server;

    basic_elog elog;

    static constexpr int cookie_max_age = 1296000;

public:
    HttpServer() {
        srand(time(0));
        elog.set_channels(websocketpp::log::elevel::info);

        server.Get("/hello", [this](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");

            bool failed;
            auto sessdata = parse_cookie(req.get_header_value("Cookie"), failed);
            if (failed) {
                res.set_content("PLEASE_LOG_IN", "text/plain");
                return;
            }

            Authorizer::Result result;
            int id; std::string user_name;
            result = auth.authorize(sessdata, id, user_name);

            if (result == Authorizer::Result::SUCCESS) {
                res.set_content("Hello, #" + std::to_string(id) + " " + user_name + "!", "text/plain");
            }
            else {
                res.set_content("PLEASE_LOG_IN", "text/plain");
            }

            }
        );


        server.Options("/login", [](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");
            res.set_header("Access-Control-Allow-Headers", "Content-Type");
            }
        );

        server.Post("/login", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Reader reader;
            Json::Value payload;
            reader.parse(req.body, payload);

            auto user_name = payload["user_name"].asString();
            auto password = payload["password"].asString();

            unsigned int sessdata;
            int id;
            Authorizer::Result result;
            result = auth.log_in(user_name, password, id, sessdata);
            if (result == Authorizer::Result::SUCCESS) {
                res.set_header("Set-Cookie", "sessdata=" + std::to_string(sessdata) + "; Max-Age=" + std::to_string(cookie_max_age));
                res.set_header("Set-Cookie", "user_name=" + user_name + "; Max-Age=" + std::to_string(cookie_max_age));
                res.set_header("Set-Cookie", "id=" + std::to_string(id) + "; Max-Age=" + std::to_string(cookie_max_age));
                res.set_content("SUCCESS", "text/plain");
                elog.write(websocketpp::log::elevel::info, "User '" + user_name + "'(#" + std::to_string(id) + ") logged in.");
            }
            else if (result == Authorizer::Result::PASSWORD_INCORRECT){
                res.set_content("PASSWORD_INCORRECT", "text/plain");
            }
            else if (result == Authorizer::Result::USER_DONOT_EXIST) {
                res.set_content("USER_DONOT_EXIST", "text/plain");
            }

            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");
            }
        );

        server.Post("/login-byid", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Reader reader;
            Json::Value payload;
            reader.parse(req.body, payload);

            auto id = payload["id"].asInt();
            auto password = payload["password"].asString();

            unsigned int sessdata;
            std::string user_name;
            Authorizer::Result result;
            result = auth.log_in(id, password, user_name, sessdata);
            if (result == Authorizer::Result::SUCCESS) {
                res.set_header("Set-Cookie", "sessdata=" + std::to_string(sessdata) + "; Max-Age=" + std::to_string(cookie_max_age));
                res.set_header("Set-Cookie", "user_name=" + user_name + "; Max-Age=" + std::to_string(cookie_max_age));
                res.set_header("Set-Cookie", "id=" + std::to_string(id) + "; Max-Age=" + std::to_string(cookie_max_age));
                res.set_content("SUCCESS", "text/plain");
                elog.write(websocketpp::log::elevel::info, "User '" + user_name + "'(#" + std::to_string(id) + ") logged in.");
            }
            else if (result == Authorizer::Result::PASSWORD_INCORRECT) {
                res.set_content("PASSWORD_INCORRECT", "text/plain");
            }
            else if (result == Authorizer::Result::USER_DONOT_EXIST) {
                res.set_content("USER_DONOT_EXIST", "text/plain");
            }

            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");
            }
        );

        server.Options("/login-byid", [](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");
            res.set_header("Access-Control-Allow-Headers", "Content-Type");
            }
        );

        server.Get("/logout", [this](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");

            bool failed;
            auto sessdata = parse_cookie(req.get_header_value("Cookie"), failed);
            if (failed) {
                res.set_content("PLEASE_LOG_IN", "text/plain");
                return;
            }

            Authorizer::Result result;
            result = auth.log_out(sessdata);

            if (result == Authorizer::Result::SUCCESS) {
                res.set_content("Successfully logged out.", "text/plain");
            }
            else {
                res.set_content("Failed to log out.", "text/plain");
            }

            }
        );

        server.Options("/register", [](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");
            res.set_header("Access-Control-Allow-Headers", "Content-Type");
            }
        );

        server.Post("/register", [this](const httplib::Request& req, httplib::Response& res) {
            Json::Reader reader;
            Json::Value payload;
            reader.parse(req.body, payload);

            auto user_name = payload["user_name"].asString();
            auto password = payload["password"].asString();

            Authorizer::Result result;
            result = auth.new_user(user_name, password);

            if (result == Authorizer::Result::SUCCESS) {
                res.set_content("SUCCESS", "text/plain");
                elog.write(websocketpp::log::elevel::info, "Created new user '" + user_name + "'.");
            }
            else if (result == Authorizer::Result::USERNAME_DUPLICATE) {
                res.set_content("USERNAME_DUPLICATE", "text/plain");
            }
            else {
                res.set_content("FAILED", "text/plain");
            }

            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");
            }
        );

        server.Options("/upload-icon", [](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");
            res.set_header("Access-Control-Allow-Headers", "Content-Type, x-requested-with");
            }
        );

        server.Post("/upload-icon", [this](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");

            bool failed;
            auto sessdata = parse_cookie(req.get_header_value("Cookie"), failed);
            if (failed) {
                res.set_content("PLEASE_LOG_IN", "text/plain");
                return;
            }

            Authorizer::Result result;
            int id; std::string user_name;
            result = auth.authorize(sessdata, id, user_name);

            if (result == Authorizer::Result::SUCCESS) {
                result = auth.set_icon_new(id, req.files.begin()->second.content.data(), req.files.begin()->second.content.length() - 1);
                if (result == Authorizer::Result::SUCCESS) {
                    res.set_content("SUCCESS", "text/plain");
                    elog.write(websocketpp::log::elevel::info, "User '" + user_name + "'(#" + std::to_string(id) + ") uploaded a new icon.");
                }
                else
                    res.set_content("FAILED", "text/plain");
            }
            else {
                res.set_content("PLEASE_LOG_IN", "text/plain");
            }

            }
        );

        server.Get("/icon", [this](const httplib::Request& req, httplib::Response& res) {
            auto icon = new std::string;

            auto it = req.params.find("id");
            if (it != req.params.end()) {
                int id = std::atoi(req.params.find("id")->second.c_str());
                auth.get_icon(id, *icon);
            }

            else if ((it = req.params.find("user_name")) != req.params.end()) {
                std::string user_name = req.params.find("user_name")->second;
                auth.get_icon(user_name, *icon);
            }

            if (!icon->empty())
                res.set_content_provider(
                    icon->size(),
                    "multipart/form-data",
                    [icon](size_t offset, size_t len, httplib::DataSink& sink) {
                        sink.write(&(*icon)[offset], len > 4 ? 4 : len);
                        return true;
                    },
                    [icon](bool) { delete icon; }
            );

            else delete icon;

            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");

            }
        );

        server.set_mount_point("/user-icon/", "./icons");

        server.Options("/set-name", [](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");
            res.set_header("Access-Control-Allow-Headers", "Content-Type, x-requested-with");
            }
        );

        server.Post("/set-name", [this](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");

            bool failed;
            auto sessdata = parse_cookie(req.get_header_value("Cookie"), failed);
            if (failed) {
                res.set_content("PLEASE_LOG_IN", "text/plain");
                return;
            }

            Authorizer::Result result;
            int id; std::string user_name;
            result = auth.authorize(sessdata, id, user_name);

            if (result == Authorizer::Result::SUCCESS) {
                Json::Value reqbody;
                Json::Reader reader;
                reader.parse(req.body, reqbody);
                result = auth.set_user_name(id, reqbody["user_name"].asString());
                if (result == Authorizer::Result::SUCCESS) {
                    res.set_content("SUCCESS", "text/plain");
                    res.set_header("Set-Cookie", "user_name=" + reqbody["user_name"].asString()
                        + "; Max-Age=" + std::to_string(cookie_max_age));
                    elog.write(websocketpp::log::elevel::info, "User '" + user_name + "'(#" + std::to_string(id)
                        + ") changed name to '" + reqbody["user_name"].asString() + "'.");
                }
                else if (result == Authorizer::Result::USERNAME_DUPLICATE)
                    res.set_content("USERNAME_DUPLICATE", "text/plain");
                else
                    res.set_content("FAILED", "text/plain");
            }
            else {
                res.set_content("PLEASE_LOG_IN", "text/plain");
            }

            }
        );

        server.Options("/friend-request", [](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");
            res.set_header("Access-Control-Allow-Headers", "Content-Type, x-requested-with");
            }
        );

        server.Post("/friend-request", [this](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");

            bool failed;
            auto sessdata = parse_cookie(req.get_header_value("Cookie"), failed);
            if (failed) {
                res.set_content("PLEASE_LOG_IN", "text/plain");
                return;
            }

            Authorizer::Result result;
            int id; std::string user_name;
            result = auth.authorize(sessdata, id, user_name);

            if (result == Authorizer::Result::SUCCESS) {
                Json::Value reqbody;
                Json::Reader reader;
                reader.parse(req.body, reqbody);
                result = auth.raise_friend_request(id, reqbody["requestee_id"].asInt());
                if (result == Authorizer::Result::ALREADY_FRIEND)
                    res.set_content("ALREADY_FRIEND", "text/plain");
                else if (result == Authorizer::Result::ALREADY_REQUESTED)
                    res.set_content("ALREADY_REQUESTED", "text/plain");
                else if (result == Authorizer::Result::USER_DONOT_EXIST)
                    res.set_content("USER_DONOT_EXIST", "text/plain");
                else if (result == Authorizer::Result::CANNOT_REQUEST_SELF)
                    res.set_content("CANNOT_REQUEST_SELF", "text/plain");
                else
                    res.set_content("SUCCESS", "text/plain");
            }
            else {
                res.set_content("PLEASE_LOG_IN", "text/plain");
            }

            }
        );

        server.Get("/get-friend-requests", [this](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");

            bool failed;
            auto sessdata = parse_cookie(req.get_header_value("Cookie"), failed);
            if (failed) {
                res.set_content("PLEASE_LOG_IN", "text/plain");
                return;
            }

            Authorizer::Result result;
            int id; std::string user_name;
            result = auth.authorize(sessdata, id, user_name);

            if (result == Authorizer::Result::SUCCESS) {
                Json::Value v;
                result = auth.get_friend_requests(id, v);
                if (result == Authorizer::Result::SUCCESS)
                    res.set_content(Json::FastWriter().write(v), "application/json");
                else 
                    res.set_content("FAILED", "text/plain");
            }
            else {
                res.set_content("PLEASE_LOG_IN", "text/plain");
            }

            }
        );

        server.Options("/handle-friend-request", [](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");
            res.set_header("Access-Control-Allow-Headers", "Content-Type, x-requested-with");
            }
        );

        server.Post("/handle-friend-request", [this](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");

            bool failed;
            auto sessdata = parse_cookie(req.get_header_value("Cookie"), failed);
            if (failed) {
                res.set_content("PLEASE_LOG_IN", "text/plain");
                return;
            }

            Authorizer::Result result;
            int id; std::string user_name;
            result = auth.authorize(sessdata, id, user_name);

            if (result == Authorizer::Result::SUCCESS) {
                Json::Value reqbody;
                Json::Reader reader;
                reader.parse(req.body, reqbody);
                if (reqbody["accept"].asBool())
                    result = auth.accept_friend_request(id, reqbody["requester_id"].asInt());
                else 
                    result = auth.remove_friend_request(id, reqbody["requester_id"].asInt());

                if(result == Authorizer::Result::SUCCESS)
                    res.set_content("SUCCESS", "text/plain");
                else 
                    res.set_content("FAILED", "text/plain");
            }
            else {
                res.set_content("PLEASE_LOG_IN", "text/plain");
            }
            
            }
        );

        server.Get("/get-friend-list", [this](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");

            bool failed;
            auto sessdata = parse_cookie(req.get_header_value("Cookie"), failed);
            if (failed) {
                res.set_content("PLEASE_LOG_IN", "text/plain");
                return;
            }

            Authorizer::Result result;
            int id; std::string user_name;
            result = auth.authorize(sessdata, id, user_name);

            if (result == Authorizer::Result::SUCCESS) {
                auto list = auth.get_friend_list(id);
                res.set_content(Json::FastWriter().write(list), "application/json");
            }
            else {
                res.set_content("PLEASE_LOG_IN", "text/plain");
            }
            
            }
        );

        server.Get("/get-chat-history", [this](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");

            bool failed;
            auto sessdata = parse_cookie(req.get_header_value("Cookie"), failed);
            if (failed) {
                res.set_content("PLEASE_LOG_IN", "text/plain");
                return;
            }

            Authorizer::Result result;
            int id; std::string user_name;
            result = auth.authorize(sessdata, id, user_name);

            if (result == Authorizer::Result::SUCCESS) {
                auto it = req.params.find("latest_timestamp");
                long long timestamp;
                if (it != req.params.end())
                    timestamp = std::atoll(it->second.c_str());
                else
                    timestamp = chat_history.get_timestamp();
                auto history = chat_history.get_chat_message(id, timestamp);
                res.set_content(Json::FastWriter().write(history), "application/json");
            }
            else {
                res.set_content("PLEASE_LOG_IN", "text/plain");
            }

            }
        );

        server.Get("/get-chat-history-20", [this](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");

            bool failed;
            auto sessdata = parse_cookie(req.get_header_value("Cookie"), failed);
            if (failed) {
                res.set_content("PLEASE_LOG_IN", "text/plain");
                return;
            }

            Authorizer::Result result;
            int id; std::string user_name;
            result = auth.authorize(sessdata, id, user_name);

            if (result == Authorizer::Result::SUCCESS) {
                auto it = req.params.find("latest_timestamp");
                auto it1 = req.params.find("friend_id");
                if (it != req.params.end() && it1 != req.params.end()) {
                    long long timestamp = std::atoll(it->second.c_str());
                    int friend_id = std::atoi(it1->second.c_str());
                    auto history = chat_history.get_20_chat_messages(id, friend_id, timestamp);
                    res.set_content(Json::FastWriter().write(history), "application/json");
                }
                else
                    res.set_content("MISS_PARAMS", "text/plain");
            }
            else {
                res.set_content("PLEASE_LOG_IN", "text/plain");
            }

            }
        );

        server.Get("/get-user-info", [this](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");

            auto it = req.params.find("id");
            if (it != req.params.end())
                res.set_content(Json::FastWriter().write(auth.get_user_info(std::atoi(it->second.c_str()))), "application/json");
            else 
                res.set_content("MISS_PARAMS", "text/plain");

            }
        );

        server.Options("/set-slogan", [](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");
            res.set_header("Access-Control-Allow-Headers", "Content-Type, x-requested-with");
            }
        );

        server.Post("/set-slogan", [this](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");

            bool failed;
            auto sessdata = parse_cookie(req.get_header_value("Cookie"), failed);
            if (failed) {
                res.set_content("PLEASE_LOG_IN", "text/plain");
                return;
            }

            Authorizer::Result result;
            int id; std::string user_name;
            result = auth.authorize(sessdata, id, user_name);

            if (result == Authorizer::Result::SUCCESS) {
                Json::Value reqbody;
                Json::Reader reader;
                reader.parse(req.body, reqbody);
                result = auth.set_slogan(id, reqbody["slogan"].asString());
                if (result == Authorizer::Result::SUCCESS) {
                    res.set_content("SUCCESS", "text/plain");
                    elog.write(websocketpp::log::elevel::info, "User '" + user_name + "'(#" + std::to_string(id)
                        + ") changed slogan to '" + reqbody["slogan"].asString() + "'.");
                }
                else
                    res.set_content("FAILED", "text/plain");
            }
            else {
                res.set_content("PLEASE_LOG_IN", "text/plain");
            }

            }
        );
    }

    void run(int port) {
        try {
            server.listen("0.0.0.0", port);
        }
        catch (const std::exception& e) {
            std::cout << e.what() << std::endl;
        }
    }

private:
    unsigned int parse_cookie(const std::string& cookie, bool& failed) {
        failed = false;
        auto pos = cookie.find("sessdata=");
        if (pos == std::string::npos) {
            failed = true;
            return 0;
        }
        pos += 9;
        auto cnt = 0;
        while (std::isdigit(cookie[pos + cnt]))++cnt;
        return std::atoll(cookie.substr(pos, cnt).c_str());
    }
};

#endif
