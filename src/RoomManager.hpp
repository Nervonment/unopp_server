#ifndef ROOM_MANAGER_HPP
#define ROOM_MANAGER_HPP

#include <unordered_map>
#include <memory>
#include <thread>
#include <mutex>

#include "Room.hpp"
#include "UnoRoom.hpp"
#include "SplendorRoom.hpp"
#include "GomokuRoom.hpp"

template <typename SendMsgFunc>
class RoomManager :public Responsor<SendMsgFunc> {
    std::unordered_map<unsigned, std::unique_ptr<Room<SendMsgFunc>>> rooms;
    std::mutex rooms_mutex;

public:
    RoomManager(const SendMsgFunc& send_msg) :Responsor<SendMsgFunc>(send_msg) {}

    void process_message(
        unsigned conn_id,
        const std::string& user_name,
        int user_id,
        const std::string& message_type,
        const Json::Value& payload
    ) {
        if (message_type == "CREATE_ROOM") {
            unsigned room_id = payload["room_id"].asUInt();
            std::string room_type = payload["room_type"].asString();
            Json::FastWriter writer;
            Json::Value res;
            res["message_type"] = "CREATE_ROOM_RES";
            
            std::lock_guard<std::mutex> lock(rooms_mutex);

            if (rooms.count(room_id)) {
                res["success"] = false;
                res["info"] = "Room " + std::to_string(room_id) + " already exists.";
                this->send_msg(conn_id, writer.write(res));
                return;
            }

            if (room_type == "UNO")
                rooms.emplace(
                    room_id,
                    std::make_unique<UnoRoom<SendMsgFunc>>(
                        this->send_msg, room_id, user_name, user_id, payload["room_name"].asString(), payload["password"].asString()
                    )
                );
            else if (room_type == "SPLENDOR") 
                rooms.emplace(
                    room_id,
                    std::make_unique<SplendorRoom<SendMsgFunc>>(
                        this->send_msg, room_id, user_name, user_id, payload["room_name"].asString(), payload["password"].asString()
                    )
                );
            else if(room_type == "GOMOKU")
                rooms.emplace(
                    room_id,
                    std::make_unique<GomokuRoom<SendMsgFunc>>(
                        this->send_msg, room_id, user_name, user_id, payload["room_name"].asString(), payload["password"].asString()
                    )
                );
            res["success"] = true;
            this->send_msg(conn_id, writer.write(res));
        }

        else if (message_type == "GET_ROOM_LIST") {
            Json::Value res;
            res["message_type"] = "ROOM_LIST";
            res["room_list"].resize(0);
            std::lock_guard<std::mutex> lock(rooms_mutex);
            for (auto& room : rooms) {
                Json::Value r;
                r["name"] = room.second->get_name();
                r["id"] = room.second->get_id();
                r["creator"] = room.second->get_creator();
                r["num_of_people"] = room.second->get_num_of_people();
                r["type"] = room.second->get_type();
                res["room_list"].append(r);
            }
            this->send_msg(conn_id, Json::FastWriter().write(res));
        }

        else {
            unsigned room_id = message_type == "JOIN_ROOM" ?
                payload["room_id"].asUInt() :
                Room<SendMsgFunc>::get_users_room_id(user_id);
            //unsigned room_id = Room<SendMsgFunc>::get_users_room_id(user_id);
            //unsigned room_id = payload["room_id"].asUInt();
            std::lock_guard<std::mutex> lock(rooms_mutex);
            if (rooms.count(room_id))
                rooms[room_id]->process_message(
                    conn_id, user_name, user_id, message_type, payload
                );
            else {
                Json::Value res;
                res["message_type"] = "ERROR";
                res["info"] = "ROOM_DONOT_EXIST";
                this->send_msg(conn_id, Json::FastWriter().write(res));
            }
        }
    }

    void process_close(unsigned conn_id, int user_id){
        auto room_id = Room<SendMsgFunc>::get_users_room_id(user_id);
        std::lock_guard<std::mutex> lock(rooms_mutex);
        if (rooms.count(room_id)) {
            auto& r = rooms[room_id];
            r->process_close(conn_id);
            // 当所有玩家离开，且房间内没有游戏进行时，关闭房间
            if (r->have_no_one_online() && !r->get_is_game_on())
                rooms.erase(room_id);
        }
    }

    void check_empty_rooms() {
        while (true) {
            auto it = rooms.begin();
            while (it != rooms.end()) {
                if (it->second->have_no_one_online()) {
                    std::lock_guard<std::mutex> lock(rooms_mutex);
                    it = rooms.erase(it);
                }
                else std::advance(it, 1);
            }
            std::this_thread::sleep_for(std::chrono::minutes(5));
        }
    }
};

#endif
