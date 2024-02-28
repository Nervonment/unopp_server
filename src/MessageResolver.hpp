#ifndef MESSAGE_RESOLVER_HPP
#define MESSAGE_RESOLVER_HPP

#include <queue>
#include <vector>
#include <unordered_map>
#include <map>
#include <iostream>
#include <string>
#include <memory>

#include <websocketpp/server.hpp>

#include <json/json.h>

#include "Room.hpp"

using websocketpp::connection_hdl;


struct Response {
    std::vector<connection_hdl> receivers;
    std::string                 msg;
};



class MessageResolver {
    std::queue<Response> to_send;
    std::map<connection_hdl, std::unique_ptr<User>, std::owner_less<connection_hdl>> users;
    std::unordered_map<unsigned, std::unique_ptr<Room>> rooms;

public:
    MessageResolver() {
    }

    void process(const std::string payload, connection_hdl sender) {
        Json::Reader reader;
        Json::FastWriter writer;
        Json::Value msg;
        if (reader.parse(payload, msg)) {
            std::string message_type = msg["message_type"].asString();

            if (message_type == "SEND_MESSAGE") {
                if (!users.count(sender)) {
                    Json::Value res;
                    res["message_type"] = "SEND_MESSAGE_RES";
                    res["success"] = false;
                    res["info"] = "User has not joined any room yet.";
                    to_send.push({
                        {sender},
                        writer.write(res)
                        });
                }
                else {
                    auto& message = msg["message"];
                    std::cout << message["user_name"].asString() << ": " << message["content"] << "\n";
                    Json::Value res;
                    res["message"] = message;
                    res["message_type"] = "NEW_USER_MESSAGE";

                    auto& room = rooms[users[sender]->room_id];
                    std::vector<connection_hdl> receivers;
                    receivers.reserve(room->users.size());
                    for (auto it : room->users)
                        receivers.push_back(it->conn_hdl);
                    to_send.push({
                        std::move(receivers),
                        writer.write(res)
                        });
                }
            }

            else if (message_type == "CREATE_ROOM") {
                Json::Value res;
                res["message_type"] = "CREATE_ROOM_RES";

                auto room_id = msg["room_id"].asUInt();
                // the room already exists
                if (rooms.count(room_id)) {
                    res["success"] = false;
                    res["info"] = "Room " + std::to_string(room_id) + " already exists.";
                    to_send.push({
                        {sender},
                        writer.write(res)
                        });
                }
                else {
                    if (msg["have_password"].asBool())
                        rooms[room_id] = std::make_unique<Room>(msg["password"].asString());
                    else
                        rooms[room_id] = std::make_unique<Room>();

                    //users[sender] = std::make_unique<User>(
                    //    msg["user_name"].asString(),
                    //    room_id,
                    //    sender
                    //);
                    std::cout << "Created room " << room_id << "." << std::endl;

                    Json::Value res;
                    res["message_type"] = "CREATE_ROOM_RES";
                    res["success"] = true;
                    to_send.push({
                        {sender},
                        writer.write(res)
                        });
                }
            }

            else if (message_type == "JOIN_ROOM") {
                Json::Value res;
                res["message_type"] = "JOIN_ROOM_RES";

                auto room_id = msg["room_id"].asUInt();

                if (!rooms.count(room_id)) {
                    res["success"] = false;
                    res["info"] = "Room " + std::to_string(room_id) + " does not exist.";
                    to_send.push({
                        {sender},
                        writer.write(res)
                        });
                }
                else {
                    users[sender] = std::make_unique<User>(
                        msg["user_name"].asString(),
                        room_id,
                        sender
                    );

                    std::string info;
                    if (!rooms[room_id]->add(users[sender].get(), msg["password"].asString(), info)) {
                        res["success"] = false;
                        res["info"] = info;
                        to_send.push({
                            {sender},
                            writer.write(res)
                            });
                    }
                    else {
                        res["success"] = true;
                        to_send.push({
                            {sender},
                            writer.write(res)
                            });
                    }
                }
            }
        }
        else {
            std::cout << "Invalid message format." << std::endl;
        }
    }

    void on_conn_close(connection_hdl hdl) {
        if (users.count(hdl)) {
            auto& room = rooms[users[hdl]->room_id];
            room->remove(users[hdl].get());
            users.erase(hdl);
        }
    }

    bool empty() {
        return to_send.empty();
    }

    Response response_next() {
        auto res = to_send.front();
        to_send.pop();
        return res;
    }
};


#endif