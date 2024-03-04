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

public:
    void process(const std::string payload, connection_hdl sender) {
        Json::Reader reader;
        Json::FastWriter writer;
        Json::Value msg;

        if (!reader.parse(payload, msg)){
            std::cout << "Invalid message format." << std::endl;
            return;
        }
        
        std::string message_type = msg["message_type"].asString();

        if (message_type == "SEND_MESSAGE") {
            if (!users.count(sender))
                fail_message(
                    message_type + "_RES",
                    "User has not joined any room yet.",
                    { sender }
            );
            else {
                auto& message = msg["message"];
                std::cout << message["user_name"].asString() << ": " << message["content"] << "\n";
                Json::Value res;
                res["message"] = message;
                res["message_type"] = "NEW_USER_MESSAGE";

                auto& room = rooms[users[sender]->room_id];
                room_broadcast(room.get(), res);
            }
        }

        else if (message_type == "CREATE_ROOM") {
            Json::Value res;
            res["message_type"] = "CREATE_ROOM_RES";

            auto room_id = msg["room_id"].asUInt();
            // the room already exists
            if (rooms.count(room_id))
                fail_message(
                    message_type + "_RES",
                    "Room " + std::to_string(room_id) + " already exists.",
                    { sender }
            );
            else {
                if (msg["have_password"].asBool())
                    rooms[room_id] = std::make_unique<Room>(msg["password"].asString());
                else
                    rooms[room_id] = std::make_unique<Room>();

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

            if (!rooms.count(room_id))
                fail_message(
                    message_type + "_RES",
                    "Room " + std::to_string(room_id) + " does not exist.",
                    { sender }
            );
            //else if (rooms[room_id]->is_playing_game()) {
            //    fail_message(
            //        message_type + "_RES",
            //        "There is a game on in this room, please wait until the game overs.",
            //        { sender }
            //    );
            //}
            else {
                auto& room = rooms[room_id];

                User* old_user_ptr = nullptr;
                bool resume_from_offline = false;
                for (auto& user : users) 
                    if (user.second->name == msg["user_name"].asString() && user.second->offline) {
                        resume_from_offline = true;
                        old_user_ptr = user.second.get();
                        break;
                    }
                
                if (!resume_from_offline && room->is_playing_game()) {
                    fail_message(
                        message_type + "_RES",
                        "There is a game on in this room, please wait until the game overs.",
                        { sender }
                    );
                    return;
                }

                if (!users.count(sender))
                    users[sender] = std::make_unique<User>(
                        msg["user_name"].asString(),
                        room_id,
                        sender
                    );

                std::string info;
                if (resume_from_offline) {
                    if (!room->resume(
                        old_user_ptr, users[sender].get(), msg["password"].asString(), info
                    )) {
                        fail_message(
                            message_type + "_RES",
                            info,
                            { sender }
                        );
                        return;
                    }
                    users.erase(old_user_ptr->conn_hdl);
                }
                else {
                    if (!room->add(users[sender].get(), msg["password"].asString(), info)) {
                        fail_message(
                            message_type + "_RES",
                            info,
                            { sender }
                        );
                        return;
                    }
                }
                
                // successed to join
                send_room_member_info(room.get());
                res["success"] = true;
                to_send.push({
                    {sender},
                    writer.write(res)
                    });

                if (room->is_playing_game()) {
                    send_cards_in_hand(room.get());
                    send_game_info(room.get());
                }
            }
        }

        else if (message_type.substr(0,3) == "UNO") {
            auto room_id = msg["room_id"].asUInt();
            if (!rooms.count(room_id)) {
                fail_message(
                    message_type + "_RES",
                    "Room " + std::to_string(room_id) + " does not exist.",
                    { sender }
                );
                return;
            }
            if (!users.count(sender)) {
                fail_message(
                    message_type + "_RES",
                    "User has not joined any room yet.",
                    { sender }
                );
                return;
            }

            auto& room = rooms[room_id];

            if (message_type == "UNO_PREPARE") {
                users[sender]->prepared = msg["prepare"].asBool();

                // update room member infomation
                send_room_member_info(room.get());

                if (room->everyone_is_prepared()
                    ) {
                    if (room->users.size() > 2) {
                        Json::Value res;
                        res["message_type"] = "UNO_START";
                        room_broadcast(room.get(), res);
                        room->start_uno_game();

                        for (auto& user : room->users)
                            user->prepared = false;

                        // send the cards in hands to the players
                        send_cards_in_hand(room.get());

                        // update game infomation
                        // broadcast
                        send_game_info(room.get());
                    }
                    else {
                        Json::Value res;
                        res["message_type"] = "UNO_BROADCAST";
                        res["type"] = "LESS_THAN_3_PEOPLE";
                        room_broadcast(room.get(), res);
                    }
                }
            }

            else if (message_type == "UNO_PLAY") {
                bool punish;
                bool success = room->play(
                    msg["user_name"].asString(),
                    msg["card"].asInt(),
                    msg["specified_color"].asInt(),
                    punish
                );
                if (success) {
                    send_cards_in_hand(room.get());
                    auto info = send_game_info(room.get());
                    send_room_member_info(room.get());
                    if (msg["card"].asInt() == 78) {
                        Json::Value res;
                        res["message_type"] = "UNO_BROADCAST";
                        res["user_name"] = msg["user_name"];
                        res["object"] = info["next_player"];
                        res["type"] = "WILD_DRAW_4";
                        room_broadcast(room.get(), res);
                    }
                    if (punish) {
                        Json::Value res;
                        res["message_type"] = "UNO_BROADCAST";
                        res["user_name"] = msg["user_name"];
                        res["type"] = "DIDNT_SAY_UNO";
                        room_broadcast(room.get(), res);
                    }
                    Json::Value res;
                    res["message_type"] = "UNO_LAST_CARD";
                    res["last_card"] = msg["card"].asInt();
                    room_broadcast(room.get(), res);
                }

                std::string winner;
                if (room->check_winner(winner)) {
                    Json::Value res;
                    res["message_type"] = "UNO_GAMEOVER";
                    res["winner"] = winner;
                    room_broadcast(room.get(), res);
                }
            }
            else if (message_type == "UNO_DRAW_ONE") {
                bool punish;
                int card;
                if (room->draw_one(msg["user_name"].asString(), punish, card)) {
                    //send_cards_in_hand(room.get());
                    send_game_info(room.get());
                    send_room_member_info(room.get());
                    Json::Value res;
                    res["message_type"] = "UNO_DRAW_ONE_RES";
                    res["success"] = true;
                    res["card"] = card;
                    to_send.push({
                        {sender},
                        writer.write(res)
                        });
                }
                if (punish) {
                    Json::Value res;
                    res["message_type"] = "UNO_BROADCAST";
                    res["user_name"] = msg["user_name"];
                    res["type"] = "SAID_UNO_BUT_DIDNT_PLAY";
                    room_broadcast(room.get(), res);
                }
            }
            else if (message_type == "UNO_SKIP_AFTER_DRAWING_ONE") {
                if (room->skip_after_drawing_one(msg["user_name"].asString())) {
                    send_game_info(room.get());
                    send_cards_in_hand(room.get());
                }
            }
            else if (message_type == "UNO_SAY_UNO") {
                Json::Value res;
                res["message_type"] = "UNO_BROADCAST";
                res["user_name"] = msg["user_name"];
                if (room->say_uno(msg["user_name"].asString()))
                    res["type"] = "SAY_UNO";
                else {
                    res["type"] = "MISSAY_UNO";
                    send_cards_in_hand(room.get());
                }
                room_broadcast(room.get(), res);
            }
            else if (message_type == "UNO_SUSPECT") {
                bool success, valid;
                std::string suspect_name;
                auto sus_cards = room->suspect(msg["user_name"].asString(), success, valid, suspect_name);
                if (valid) {
                    {
                        Json::Value res;
                        res["message_type"] = "UNO_SUSPECT_CARDS";
                        res["cards"].resize(0);
                        for (auto& card : sus_cards)
                            res["cards"].append(card);
                        to_send.push({
                            {sender},
                            writer.write(res)
                            });
                    }

                    Json::Value res;
                    res["message_type"] = "UNO_BROADCAST";
                    res["user_name"] = msg["user_name"];
                    res["suspect"] = suspect_name;
                    res["type"] = "SUSPECT";
                    res["success"] = success;
                    room_broadcast(room.get(), res);
                    
                    send_cards_in_hand(room.get());
                    send_game_info(room.get());
                    send_room_member_info(room.get());
                }
            }
            else if (message_type == "UNO_DISSUSPECT") {
                if (room->dissuspect(msg["user_name"].asString())) {
                    send_cards_in_hand(room.get());
                    send_game_info(room.get());
                    send_room_member_info(room.get());
                }
            }
        }
    }

    void on_conn_close(connection_hdl hdl) {
        if (users.count(hdl)) {
            auto& user = users[hdl];
            auto& room = rooms[user->room_id];
            if (room->users.size() == 1 && !room->is_playing_game()) {
                for (auto& u : room->users)
                    if (u == user.get())
                        rooms.erase(user->room_id);
                users.erase(hdl); 
            }
            else if (room->is_playing_game()) {
                int online_cnt = 0;
                for (auto& user : room->users)
                    if (!user->offline)
                        ++online_cnt;
                if (online_cnt > 1) {
                    user->offline = true;
                    send_room_member_info(room.get());
                }
                else {
                    auto room_id = user->room_id;
                    for (auto& user : rooms[room_id]->users)
                        users.erase(user->conn_hdl);
                    rooms.erase(room_id);
                }
            }
            else {
                room->remove(users[hdl].get());
                users.erase(hdl);
            }
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

private:
    void fail_message(
        const std::string& mesage_type, 
        const std::string& info, 
        const std::vector<connection_hdl>& receivers
    ) {
        Json::Value res;
        Json::FastWriter writer;

        res["message_type"] = mesage_type;
        res["success"] = false;
        res["info"] = info;
        to_send.push({
            receivers,
            writer.write(res)
            });
    }

    void room_broadcast(
        Room* room,
        const Json::Value& payload
    ) {
        Json::FastWriter writer;

        std::vector<connection_hdl> receivers;
        receivers.reserve(room->users.size());
        for (auto it : room->users)
            if (!it->offline)
                receivers.push_back(it->conn_hdl);
        to_send.push({
                std::move(receivers),
                writer.write(payload)
            });
    }

    void send_cards_in_hand(Room* room) {
        Json::FastWriter writer;

        for (auto& user : room->users) {
            auto cards = room->get_cards_in_hand(user);
            Json::Value res;
            res["message_type"] = "UNO_CARDS_IN_HAND";
            res["cards"] = cards;
            to_send.push({
                {user->conn_hdl},
                writer.write(res)
                });
        }
    }

    void send_room_member_info(Room* room) {
        Json::Value res;
        res["message_type"] = "ROOM_MEMBERS_INFO";
        for (auto user : room->users) {
            Json::Value member;
            member["name"] = user->name;
            member["prepared"] = user->prepared;
            member["offline"] = user->offline;
            member["card_count"] = room->is_playing_game() ? room->get_player_card_count(user->name) : 0;
            res["members"].append(member);
        }
        room_broadcast(room, res);
    }

    Json::Value send_game_info(Room* room) {
        auto info = room->get_game_info();
        info["message_type"] = "UNO_GAME_INFO";
        room_broadcast(room, info);
        return info;
    }
};


#endif