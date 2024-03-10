#ifndef ROOM_MANAGER_HPP
#define ROOM_MANAGER_HPP

#include <unordered_map>
#include <memory>

#include "Room.hpp"
#include "UnoRoom.hpp"

template <typename SendMsgFunc>
class RoomManager :public Responsor<SendMsgFunc> {
    std::unordered_map<unsigned, std::unique_ptr<Room<SendMsgFunc>>> rooms;

public:
    RoomManager(const SendMsgFunc& send_msg) :Responsor<SendMsgFunc>(send_msg) {}

    void process_message(
        unsigned conn_id,
        const std::string& user_name,
        const std::string& message_type,
        const Json::Value& payload
    ) {
        if (message_type == "CREATE_ROOM") {
            unsigned room_id = payload["room_id"].asUInt();
            Json::FastWriter writer;
            Json::Value res;
            res["message_type"] = "CREATE_ROOM_RES";
            
            if (rooms.count(room_id)) {
                res["success"] = false;
                res["info"] = "Room " + std::to_string(room_id) + " already exists.";
                this->send_msg(conn_id, writer.write(res));
                return;
            }

            rooms.emplace(
                room_id, 
                std::make_unique<UnoRoom<SendMsgFunc>>(
                    this->send_msg, room_id, payload["password"].asString()
                )
            );
            res["success"] = true;
            this->send_msg(conn_id, writer.write(res));
        }

        else {
            unsigned room_id = payload["room_id"].asUInt();
            if (rooms.count(room_id))
                rooms[room_id]->process_message(
                    conn_id, user_name, message_type, payload
                );
            else {
                Json::Value res;
                res["message_type"] = "ERROR";
                res["info"] = "ROOM_DONOT_EXIST";
                this->send_msg(conn_id, Json::FastWriter().write(res));
            }
        }
    }

    void process_close(unsigned conn_id, const std::string& user_name){
        auto room_id = Room<SendMsgFunc>::get_users_room_id(user_name);
        if (rooms.count(room_id)) {
            rooms[room_id]->process_close(conn_id);
            if (rooms[room_id]->have_no_one_online())
                rooms.erase(room_id);
        }
    }
};

#endif
