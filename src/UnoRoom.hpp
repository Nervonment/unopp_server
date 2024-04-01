#ifndef UNO_ROOM_HPP
#define UNO_ROOM_HPP

#include "Room.hpp"
#include "Uno.hpp"
#include <memory>

template <typename SendMsgFunc>
class UnoRoom :public Room<SendMsgFunc> {
    std::unique_ptr<Uno> uno_game;

public:
    UnoRoom(
        const SendMsgFunc& send_msg,
        unsigned id, const std::string& creator,
        const std::string& name, const std::string& password
    )
        :Room<SendMsgFunc>(send_msg, id, creator, name, password) {}

    virtual std::string get_type() {
        return "UNO";
    }

    virtual void process_message(
        unsigned conn_id,
        const std::string& user_name,
        int                user_id,
        const std::string& message_type,
        const Json::Value& payload
    ) {
        if (message_type.substr(0, 3) == "UNO") {
            if (message_type == "UNO_PLAY") {
                bool punish;
                bool success = uno_game->play(
                    user_id,
                    int_2_card(payload["card"].asInt()),
                    static_cast<Uno::CardColor>(payload["specified_color"].asInt()),
                    punish
                );
                if (success) {
                    send_cards_in_hand();
                    send_game_info();
                    //send_room_member_info();

                    auto info = get_game_info();
                    if (payload["card"].asInt() == 78) {
                        Json::Value res;
                        res["user_name"] = user_name;
                        res["object"] = info["next_player"];
                        res["type"] = "WILD_DRAW_4";
                        this->broadcast(res, "UNO_BROADCAST");
                    }
                    if (punish) {
                        Json::Value res;
                        res["message_type"] = "UNO_BROADCAST";
                        res["user_name"] = user_name;
                        res["type"] = "DIDNT_SAY_UNO";
                        this->broadcast(res, "UNO_BROADCAST");
                    }
                    Json::Value res;
                    res["message_type"] = "UNO_LAST_CARD";
                    res["last_card"] = payload["card"].asInt();
                    this->broadcast(res, "UNO_LAST_CARD");
                }

                int winner;
                if (uno_game->check_winner(winner)) {
                    Json::Value res;
                    res["message_type"] = "UNO_GAMEOVER";
                    res["winner"]["id"] = winner;
                    res["winner"]["name"] = this->user_id_to_user_name(winner);
                    res["result"] = get_game_result();
                    this->broadcast(res, "UNO_GAMEOVER");
                    this->is_game_on = false;
                    this->broadcast(this->get_room_members_info(), "ROOM_MEMBERS_INFO");
                }
            }

            else if (message_type == "UNO_DRAW_ONE") {
                bool punish;
                Uno::Card card;
                if (uno_game->draw_one(
                    user_id, 
                    punish, 
                    card
                )) {
                    send_game_info();
                    Json::Value res;
                    res["message_type"] = "UNO_DRAW_ONE_RES";
                    res["success"] = true;
                    res["card"] = card_2_int(card);
                    this->send_msg(conn_id, Json::FastWriter().write(res));
                }
                if (punish) {
                    Json::Value res;
                    res["user_name"] = user_name;
                    res["type"] = "SAID_UNO_BUT_DIDNT_PLAY";
                    this->broadcast(res, "UNO_BROADCAST");
                }
            }

            else if (message_type == "UNO_SKIP_AFTER_DRAWING_ONE") {
                if (uno_game->skip_after_drawing_one(user_id)) {
                    send_game_info();
                    send_cards_in_hand();
                }
            }

            else if (message_type == "UNO_SAY_UNO") {
                Json::Value res;
                res["user_name"] = user_name;
                if (uno_game->say_uno(user_id))
                    res["type"] = "SAY_UNO";
                else {
                    res["type"] = "MISSAY_UNO";
                    send_cards_in_hand();
                    send_game_info();
                }
                this->broadcast(res, "UNO_BROADCAST");
            }

            else if (message_type == "UNO_SUSPECT") {
                bool success, valid;
                int suspect_id;
                auto sus_cards = uno_game->suspect(
                    user_id, success, valid, suspect_id
                );
                if (valid) {
                    {
                        Json::Value res;
                        res["message_type"] = "UNO_SUSPECT_CARDS";
                        res["cards"].resize(0);
                        for (auto& card : sus_cards)
                            res["cards"].append(card_2_int(card));
                        this->send_msg(conn_id, Json::FastWriter().write(res));
                    }

                    Json::Value res;
                    res["message_type"] = "UNO_BROADCAST";
                    res["user_name"] = user_name;
                    res["suspect"] = this->user_id_to_user_name(suspect_id);
                    res["type"] = "SUSPECT";
                    res["success"] = success;
                    this->broadcast(res, "UNO_BROADCAST");

                    send_cards_in_hand();
                    send_game_info();
                }
            }

            else if (message_type == "UNO_DISSUSPECT") {
                if (uno_game->dissuspect(user_id)) {
                    send_cards_in_hand();
                    send_game_info();
                }
            }
        }

        else {
            Room<SendMsgFunc>::process_message(conn_id, user_name, user_id, message_type, payload);
            if (message_type == "JOIN_ROOM" && this->is_game_on && this->connections.count(conn_id)) {
                send_cards_in_hand();
                send_game_info();
            }
        }
    }

    virtual void on_everyone_prepared() {
        if (this->connections.size() < 3) {
            Json::Value res;
            res["type"] = "LESS_THAN_3_PEOPLE";
            this->broadcast(res, "UNO_BROADCAST");
        }
        else if (this->connections.size() > 10) {
            Json::Value res;
            res["type"] = "MORE_THAN_10_PEOPLE";
            this->broadcast(res, "UNO_BROADCAST");
        }
        else {
            this->is_game_on = true;

            std::vector<int> players;
            players.reserve(this->connections.size());
            for (auto& p : this->connections)
                players.emplace_back(p.second.user_id);
            uno_game = std::make_unique<Uno>(players);

            this->broadcast(Json::Value(), "UNO_START");

            for (auto& p : this->connections)
                p.second.prepared = false;

            // Send the cards in hands to the players
            send_cards_in_hand();

            // Update game infomation
            // Broadcast
            send_game_info();

            this->broadcast(this->get_room_members_info(), "ROOM_MEMBERS_INFO");
        }
    }

    void send_cards_in_hand() {
        for (auto p : this->connections) {
            auto cards = get_cards_in_hand(p.second.user_id);
            Json::Value res;
            res["message_type"] = "UNO_CARDS_IN_HAND";
            res["cards"] = cards;
            this->send_msg(p.first, Json::FastWriter().write(res));
        }
    }

    void send_game_info() {
        this->broadcast(get_game_info(), "UNO_GAME_INFO");
    }

    // stupid O(n) find
    // will be optimized to unordered_map later
    Json::Value get_cards_in_hand(int user_id) {
        const auto& player = *std::find_if(
            uno_game->players.begin(),
            uno_game->players.end(),
            [&user_id](const Uno::Player& p) {
                return p.user_id == user_id;
            }
        );
        Json::Value cards;
        cards.resize(0);
        for (auto card : player.cards_in_hand) {
            cards.append(Json::Value(card_2_int(card)));
        }
        return cards;
    }

    Json::Value get_game_info() {
        Json::Value info;
        info["last_card"] = card_2_int(uno_game->get_last_card());
        info["next_player"] = this->user_id_to_user_name(uno_game->get_next_player());
        info["specified_color"] = uno_game->get_specified_color();
        info["direction"] = uno_game->get_direction();
        info["players"].resize(0);
        for (auto& player : uno_game->players) {
            Json::Value p;
            p["name"] = this->user_id_to_user_name(player.user_id);
            p["id"] = player.user_id;
            p["count"] = player.cards_in_hand.size();
            info["players"].append(p);
        }
        return info;
    }

    Json::Value get_game_result() {
        Json::Value info;
        info["last_card"] = card_2_int(uno_game->get_last_card());
        info["players"].resize(0);
        for (auto& player : uno_game->players) {
            Json::Value p;
            p["name"] = this->user_id_to_user_name(player.user_id);
            p["id"] = player.user_id;
            p["cards"].resize(0);
            for (auto& card : player.cards_in_hand)
                p["cards"].append(card_2_int(card));
            p["count"] = player.cards_in_hand.size();
            info["players"].append(p);
        }
        return info;
    }

    int card_2_int(Uno::Card card) {
        return card.content + card.color * 16;
    }
    Uno::Card int_2_card(int card_hash) {
        return {
            static_cast<Uno::CardColor>(card_hash / 16),
            static_cast<Uno::CardContent>(card_hash % 16)
        };
    }
};

#endif
