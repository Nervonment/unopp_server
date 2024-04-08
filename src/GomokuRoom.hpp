#ifndef GOMOKU_ROOM_HPP
#define GOMOKU_ROOM_HPP

#include "Room.hpp"
#include "Gomoku.hpp"
#include <utility>
#include <cstdlib>
#include <ctime>

template <typename SendMsgFunc>
class GomokuRoom :public Room<SendMsgFunc> {
    Gomoku game;
    std::array<std::pair<int, bool>, 2> user_id_to_is_black;

public:
    GomokuRoom(
        const SendMsgFunc& send_msg,
        unsigned id, const std::string& creator, int creator_id,
        const std::string& name, const std::string& password
    )
        :Room<SendMsgFunc>(send_msg, id, creator, creator_id, name, password) {
    }

    virtual std::string get_type() {
        return "GOMOKU";
    }

    virtual void process_message(
        unsigned conn_id,
        const std::string& user_name,
        int                user_id,
        const std::string& message_type,
        const Json::Value& payload
    ) {
        if (message_type == "GOMOKU_DROP") {
            bool is_black =
                (user_id_to_is_black[0].first == user_id ? user_id_to_is_black[0] : user_id_to_is_black[1])
                .second;
            bool can_drop = game.drop(payload["y"].asInt(), payload["x"].asInt(), is_black);
            if (can_drop) {
                send_game_info();
                std::thread t(
                    [&] {
                        if (this->connections.size() == 1)
                            game.update();
                        // 你没有看错，确实要更新两次
                        game.update();
                        send_game_info();
                        auto status = game.get_status();
                        if (status != Gomoku::NOT_END) {
                            this->is_game_on = false;
                            send_game_result(status == Gomoku::BLACK_WIN, status == Gomoku::TIED);
                        }
                    }
                );
                t.detach();
            }
        }

        else {
            Room<SendMsgFunc>::process_message(conn_id, user_name, user_id, message_type, payload);
            if (message_type == "JOIN_ROOM" && this->is_game_on && this->connections.count(conn_id))
                send_game_info();
        }
    }

    virtual void on_everyone_prepared() {
        if (this->connections.size() > 2) {
            Json::Value res;
            res["type"] = "MORE_THAN_2_PEOPLE";
            this->broadcast(res, "GOMOKU_BROADCAST");
        }
        else {
            game.clear();

            if (this->connections.size() == 1) {
                user_id_to_is_black[0].first = this->connections.begin()->second.user_id;
                user_id_to_is_black[0].second = true;
                user_id_to_is_black[1].first = 0;
                game.enable_ai(true);
                Json::Value res;
                res["type"] = "PLAY_WITH_ALGORITHM";
                this->broadcast(res, "GOMOKU_BROADCAST");
            }

            else {
                bool i = rand() % 2;
                for (auto& p : this->connections) {
                    user_id_to_is_black[i].first = p.second.user_id;
                    user_id_to_is_black[i].second = i;
                    i = !i;
                }
                game.enable_ai(false);
            }

            this->is_game_on = true;

            this->broadcast(Json::Value(), "GOMOKU_START");

            for (auto& p : this->connections)
                p.second.prepared = false;

            // Update game infomation
            // Broadcast
            send_game_info();
            this->broadcast(this->get_room_members_info(), "ROOM_MEMBERS_INFO");
        }
    }

    void send_game_info() {
        auto info = game.get_game_info();
        info["players"].resize(2);
        info["players"][0]["id"] = user_id_to_is_black[0].first;
        info["players"][0]["name"] = this->user_id_to_user_name(user_id_to_is_black[0].first);
        info["players"][0]["is_black"] = user_id_to_is_black[0].second;
        if (user_id_to_is_black[1].first)
            info["players"][1]["id"] = user_id_to_is_black[1].first;
        else info["players"][1]["id"] = "robot";
        info["players"][1]["is_black"] = user_id_to_is_black[1].second;
        info["players"][1]["name"] = user_id_to_is_black[1].first ? 
            this->user_id_to_user_name(user_id_to_is_black[1].first) : "AlphaGomoku";
        this->broadcast(info, "GOMOKU_GAME_INFO");
    }

    void send_game_result(bool winner_is_black, bool tied) {
        Json::Value res;
        if (!tied)
            res["winner"] = winner_is_black ? "BLACK" : "WHITE";
        else res["winner"] = "TIED";
        this->broadcast(res, "GOMOKU_GAME_OVER");
    }
};

#endif
