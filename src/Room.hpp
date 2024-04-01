#ifndef ROOM_HPP
#define ROOM_HPP

#include <string>
#include <unordered_map>
#include <json/json.h>

#include "Responsor.hpp"

struct UserInfo {
    std::string user_name;
    int         user_id;
    bool offline    = false;
    bool prepared   = false;
};

// Basic room class
// Process chat messages
template <typename SendMsgFunc>
class Room:public Responsor<SendMsgFunc> {
    unsigned id;
    std::string creator;
    std::string name;
    inline static std::unordered_map<int, unsigned> user_2_room_id{};
    std::string password;

protected:
    std::unordered_map<unsigned, UserInfo> connections;
    bool is_game_on = false;

public:
    unsigned get_id() {
        return this->id;
    }
    std::string get_creator() {
        return this->creator;
    }
    std::string get_name() {
        return this->name;
    }
    int get_num_of_people() {
        return this->connections.size();
    }
    virtual std::string get_type() {
        return "Chat Room";
    }

    std::string user_id_to_user_name(int id) {
        for (auto& conn : connections)
            if (conn.second.user_id == id)
                return conn.second.user_name;
        return "";
    }

public:
    Room(
        const SendMsgFunc& send_msg,
        unsigned id, const std::string& creator,
        const std::string& name, const std::string& password
    )
        :Responsor<SendMsgFunc>(send_msg), id(id), creator(creator), name(name), password(password) {}

    virtual void process_message(
        unsigned conn_id,
        const std::string& user_name,
        int                user_id,
        const std::string& message_type,
        const Json::Value& payload
    ) {
        if (message_type == "JOIN_ROOM") {
            Json::Value res;
            res["message_type"] = message_type + "_RES";
            
            bool erase_old_conn_id = false;
            unsigned old_conn_id;

            if (is_game_on) {
                bool ok = false;
                for(auto& p: connections)
                    if (p.second.user_name == user_name) {
                        old_conn_id = p.first;
                        ok = erase_old_conn_id = true; 
                        break;
                    }
                if (!ok) {
                    res["success"] = false;
                    res["info"] = "There is a game on in this room, please wait until the game overs.";
                    this->send_msg(conn_id, Json::FastWriter().write(res));
                    return;
                }
            }

            if (user_2_room_id.count(user_id) && !erase_old_conn_id
                //&& user_2_room_id[user_id] != id
                ) {
                res["success"] = false;
                res["info"] = "You have already joined room " + std::to_string(user_2_room_id[user_id]);
                this->send_msg(conn_id, Json::FastWriter().write(res));
                return;
            }

            if (payload["password"].asString() != password) {
                res["success"] = false;
                res["info"] = "Password incorrect.";
                this->send_msg(conn_id, Json::FastWriter().write(res));
                return;
            }

            if(erase_old_conn_id)
                connections.erase(old_conn_id); // Erase the old connection id
            else {                              
                Json::Value res;
                res["user_name"] = user_name;
                broadcast(res, "NEW_MEMBER");   // Real new member, not restored from offline
            }

            connections[conn_id].offline = false;
            connections[conn_id].prepared = false;
            connections[conn_id].user_name = user_name;
            connections[conn_id].user_id = user_id;
            user_2_room_id[user_id] = id;
            res["success"] = true;
            res["room_type"] = get_type();
            this->send_msg(conn_id, Json::FastWriter().write(res));
            broadcast(get_room_members_info(), "ROOM_MEMBERS_INFO");
        }

        else {
            if (!connections.count(conn_id)) {
                Json::Value res;
                res["success"] = false;
                res["info"] = "Please join the room first.";
                this->send_msg(conn_id, Json::FastWriter().write(res));
                return;
            }

            if (message_type == "CHAT_MESSAGE") {
                auto res = payload;
                res["message"]["user_name"] = user_name;
                res["message"]["user_id"] = user_id;
                broadcast(res, "CHAT_MESSAGE");
            }

            else if (message_type == "GAME_PREPARE") {
                if (connections.count(conn_id))
                    connections[conn_id].prepared = payload["prepare"].asBool();
                broadcast(get_room_members_info(), "ROOM_MEMBERS_INFO");
                bool everyone_prepared = true;
                for (auto p : connections)
                    if (!p.second.prepared)
                        everyone_prepared = false;
                if (everyone_prepared)
                    on_everyone_prepared();
            }
        }
    }

    virtual void on_everyone_prepared() {}

    virtual void process_close(unsigned conn_id) {
        if (connections.count(conn_id)) {
            if (is_game_on)
                connections[conn_id].offline = true;
            else {
                Json::Value res;
                res["user_name"] = connections[conn_id].user_name;
                res["user_id"] = connections[conn_id].user_id;
                broadcast(res, "MEMBER_LEAVES");   
                user_2_room_id.erase(connections[conn_id].user_id);
                connections.erase(conn_id);
            }
        }
        broadcast(get_room_members_info(), "ROOM_MEMBERS_INFO");
    }

    void broadcast(Json::Value payload, const std::string& message_type) {
        payload["message_type"] = message_type;
        Json::FastWriter writer;
        std::string payload_str = writer.write(payload);
        for (auto& p : connections)
            this->send_msg(p.first, payload_str);
    }

    Json::Value get_room_members_info() {
        Json::Value info;
        info["members"].resize(0);
        for (auto p : connections) {
            Json::Value user;
            user["name"] = p.second.user_name;
            user["id"] = p.second.user_id;
            user["prepared"] = p.second.prepared;
            user["offline"] = p.second.offline;
            info["members"].append(user);
        }
        return info;
    }

    bool have_no_one_online() {
        bool res = true;
        for (auto& p : connections)
            if (!p.second.offline)
                res = false;
        return res;
    }

    static unsigned get_users_room_id(int user_id) {
        return user_2_room_id.count(user_id) ?
            user_2_room_id[user_id] : 0;
    }


    ~Room() {
        for (auto& p : connections)
            user_2_room_id.erase(p.second.user_id);
    }
};


#endif
