#ifndef SPLENDOR_ROOM_HPP
#define SPLENDOR_ROOM_HPP

#include "Room.hpp"
#include "Splendor.hpp"
#include <memory>

template <typename SendMsgFunc>
class SplendorRoom :public Room<SendMsgFunc> {
    std::unique_ptr<Splendor> game;

public:
    SplendorRoom(
        const SendMsgFunc& send_msg,
        unsigned id, const std::string& creator, int creator_id,
        const std::string& name, const std::string& password
    )
        :Room<SendMsgFunc>(send_msg, id, creator, creator_id, name, password) {}

    virtual std::string get_type() {
        return "SPLENDOR";
    }

    virtual void process_message(
        unsigned conn_id,
        const std::string& user_name,
        int                user_id,
        const std::string& message_type,
        const Json::Value& payload
    ) {
        if (message_type.substr(0, 8) == "SPLENDOR") {
            if (message_type == "SPLENDOR_TAKE_2") {
                int mine_idx = payload["mine"].asInt();
                bool success = game->take_2_mines(
                    static_cast<Splendor::Mine>(mine_idx),
                    user_id
                );
                if (success) 
                    send_game_info();
            }

            else if (message_type == "SPLENDOR_TAKE_3") {
                auto& mines = payload["mines"];
                if (mines.isArray() && mines.size() == 3) {
                    bool success = game->take_3_mines(
                        {
                            static_cast<Splendor::Mine>(mines[0].asInt()),
                            static_cast<Splendor::Mine>(mines[1].asInt()),
                            static_cast<Splendor::Mine>(mines[2].asInt()),
                        },
                        user_id
                        );
                    if (success) 
                        send_game_info();
                }
            }

            else if (message_type == "SPLENDOR_BUY_COUPON") {
                bool success = game->buy_coupon(payload["coupon_idx"].asInt(), user_id);
                if (success) {
                    send_game_info();
                    int winner;
                    if (game->check_winner(winner)) {
                        this->is_game_on = false;
                        send_game_result(winner);
                    }
                }
            }

            else if (message_type == "SPLENDOR_RESERVE_COUPON") {
                bool success = game->reserve_coupon(payload["coupon_idx"].asInt(), user_id);
                if (success)
                    send_game_info();
            }

            else if (message_type == "SPLENDOR_BUY_RESERVED_COUPON") {
                bool success = game->buy_reserved_coupon(payload["coupon_idx"].asInt(), user_id);
                if (success) {
                    send_game_info();
                    int winner;
                    if (game->check_winner(winner)) {
                        this->is_game_on = false;
                        send_game_result(winner);
                    }
                }
            }

            else if (message_type == "SPLENDOR_RETURN_MINE") {
                bool success = game->return_mine(
                    static_cast<Splendor::Mine>(payload["mine"].asInt()),
                    user_id
                );
                if (success)
                    send_game_info();
            }
        }

        else {
            Room<SendMsgFunc>::process_message(conn_id, user_name, user_id, message_type, payload);
            if (message_type == "JOIN_ROOM" && this->is_game_on && this->connections.count(conn_id))
                send_game_info();
        }
    }

    virtual void on_everyone_prepared() {
        if (this->connections.size() < 2) {
            Json::Value res;
            res["type"] = "LESS_THAN_2_PEOPLE";
            this->broadcast(res, "SPLENDOR_BROADCAST");
        }
        else if (this->connections.size() > 4) {
            Json::Value res;
            res["type"] = "MORE_THAN_4_PEOPLE";
            this->broadcast(res, "SPLENDOR_BROADCAST");
        }
        else {
            this->is_game_on = true;

            std::vector<int> players;
            players.reserve(this->connections.size());
            for (auto& p : this->connections)
                players.emplace_back(p.second.user_id);
            game = std::make_unique<Splendor>(players);

            this->broadcast(Json::Value(), "SPLENDOR_START");

            for (auto& p : this->connections)
                p.second.prepared = false;

            // Update game infomation
            // Broadcast
            send_game_info();
            this->broadcast(this->get_room_members_info(), "ROOM_MEMBERS_INFO");
        }
    }

    void send_game_info() {
        auto info = game->get_game_info();
        for (auto& p : info["players"])
            p["name"] = this->user_id_to_user_name(p["id"].asInt());
        if (info["last_action"].isMember("subject_id"))
            info["last_action"]["subject_name"] = this->user_id_to_user_name(info["last_action"]["subject_id"].asInt());
        if (!info["ally_actions"].isNull())
            for (auto& a : info["ally_actions"])
                a["subject_name"] = this->user_id_to_user_name(a["subject_id"].asInt());
        for (auto& p : this->connections) {
            Json::Value res;
            res["message_type"] = "SPLENDOR_GAME_INFO";
            res["info"] = info;
            res["info"]["player_info"] = game->get_player_info(p.second.user_id);
            this->send_msg(p.first, Json::FastWriter().write(res));
        }
    }

    void send_game_result(int winner) {
        auto info = game->get_game_info();
        for (auto& p : info["players"])
            p["name"] = this->user_id_to_user_name(p["id"].asInt());
        for (auto& p : this->connections) {
            Json::Value res;
            res["message_type"] = "SPLENDOR_GAME_OVER";
            res["info"] = info;
            res["winner_id"] = winner;
            res["winner_name"] = this->user_id_to_user_name(winner);
            res["info"]["player_info"] = game->get_player_info(p.second.user_id);
            this->send_msg(p.first, Json::FastWriter().write(res));
        }
    }
};

#endif
