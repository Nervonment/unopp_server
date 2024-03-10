#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include <vector>
#include <string>
#include <mutex>

#include <json/json.h>
#include <httplib.h>

#include "Authorizer.hpp"

class HttpServer {
    Authorizer auth;
    std::mutex auth_mutex;

    httplib::Server server;

    static constexpr int cookie_max_age = 1296000;

public:
    HttpServer() {
        srand(time(0));

        server.Get("/hello", [this](const httplib::Request& req, httplib::Response& res) {
            auto sessdata = parse_cookie(req.get_header_value("Cookie"));

            Authorizer::Result result;
            int id; std::string user_name;
            {
                std::lock_guard<std::mutex> guard(auth_mutex);
                result = auth.authorize(sessdata, id, user_name);
            }

            if (result == Authorizer::Result::SUCCESS) {
                res.set_content("Hello, #" + std::to_string(id) + " " + user_name + "!", "text/plain");
            }
            else {
                res.set_content("Please log in first!", "text/plain");
            }

            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");
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
            {
                std::lock_guard<std::mutex> guard(auth_mutex);
                result = auth.log_in(user_name, password, id, sessdata);
            }
            if (result == Authorizer::Result::SUCCESS) {
                res.set_header("Set-Cookie", "sessdata=" + std::to_string(sessdata) + "; Max-Age=" + std::to_string(cookie_max_age));
                res.set_header("Set-Cookie", "user_name=" + user_name + "; Max-Age=" + std::to_string(cookie_max_age));
                res.set_header("Set-Cookie", "id=" + std::to_string(id) + "; Max-Age=" + std::to_string(cookie_max_age));
                res.set_content("SUCCESS", "text/plain");
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

        server.Get("/logout", [this](const httplib::Request& req, httplib::Response& res) {
            auto sessdata = parse_cookie(req.get_header_value("Cookie"));

            Authorizer::Result result;
            {
                std::lock_guard<std::mutex> guard(auth_mutex);
                result = auth.log_out(sessdata);
            }

            if (result == Authorizer::Result::SUCCESS) {
                res.set_content("Successfully logged out.", "text/plain");
            }
            else {
                res.set_content("Failed to log out.", "text/plain");
            }

            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");
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
            {
                std::lock_guard<std::mutex> guard(auth_mutex);
                result = auth.new_user(user_name, password);
            }

            if (result == Authorizer::Result::SUCCESS) {
                res.set_content("SUCCESS", "text/plain");
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
            auto sessdata = parse_cookie(req.get_header_value("Cookie"));

            Authorizer::Result result;
            int id; std::string user_name;
            {
                std::lock_guard<std::mutex> guard(auth_mutex);
                result = auth.authorize(sessdata, id, user_name);

                if (result == Authorizer::Result::SUCCESS) {
                    result = auth.set_icon(id, req.files.begin()->second.content);
                    if (result == Authorizer::Result::SUCCESS)
                        res.set_content("SUCCESS", "text/plain");
                    else
                        res.set_content("FAILED", "text/plain");
                }
                else {
                    res.set_content("PLEASE_LOG_IN", "text/plain");
                }
            }

            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");

            }
        );

        server.Get("/icon", [this](const httplib::Request& req, httplib::Response& res) {
            auto icon = new std::string;

            auto it = req.params.find("id");
            {
                std::lock_guard<std::mutex> guard(auth_mutex);
                if (it != req.params.end()) {
                    int id = std::atoi(req.params.find("id")->second.c_str());
                    auth.get_icon(id, *icon);
                }

                else if ((it = req.params.find("user_name")) != req.params.end()) {
                    std::string user_name = req.params.find("user_name")->second;
                    auth.get_icon(user_name, *icon);
                }
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

        server.Options("/set-name", [](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");
            res.set_header("Access-Control-Allow-Headers", "Content-Type, x-requested-with");
            }
        );

        server.Post("/set-name", [this](const httplib::Request& req, httplib::Response& res) {
            auto sessdata = parse_cookie(req.get_header_value("Cookie"));

            Authorizer::Result result;
            int id; std::string user_name;
            {
                std::lock_guard<std::mutex> guard(auth_mutex);
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
                    }
                    else if(result == Authorizer::Result::USERNAME_DUPLICATE)
                        res.set_content("USERNAME_DUPLICATE", "text/plain");
                    else 
                        res.set_content("FAILED", "text/plain");
                }
                else {
                    res.set_content("PLEASE_LOG_IN", "text/plain");
                }
            }

            res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin"));
            res.set_header("Access-Control-Allow-Credentials", "true");

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
    unsigned int parse_cookie(const std::string& cookie) {
        auto pos = cookie.find("sessdata=") + 9;
        auto cnt = 0;
        while (std::isdigit(cookie[pos + cnt]))++cnt;
        return std::atoll(cookie.substr(pos, cnt).c_str());
    }
};

#endif
