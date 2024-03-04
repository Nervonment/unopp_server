#ifndef ROOM_HPP
#define ROOM_HPP

#include <string>
#include <vector>
#include <memory>

#include <websocketpp/server.hpp>

#include <json/json.h>

#include "Uno.hpp"

using websocketpp::connection_hdl;

struct User {
    std::string     name;
    unsigned        room_id;
    connection_hdl  conn_hdl;

    bool            prepared = false;
    bool            offline = false;

    User(const std::string& name, unsigned room_id, connection_hdl conn_hdl) :
        name(name), room_id(room_id), conn_hdl(conn_hdl) {
    }
};

struct Room {
    std::vector<User*>      users;
    bool                    have_password = false;
    std::string             password;

private:
    bool                    uno_game_started = false;
    std::unique_ptr<Uno>    uno_game;

public:
    Room() {
    }
    Room(const std::string& password) :
        have_password(true), password(password) {
    }

    bool add(User* user, const std::string& password, std::string& info) {
        if (have_password && password != this->password) {
            info = "Password incorrect. ";
            return false;
        }
        for (auto u : users) 
            if (u->name == user->name) {
                info = "User '"+user->name+"' is already in the room. Please Select another name. ";
                return false;
            }
        users.push_back(user);
        return true;
    }

    void remove(User* user) {
        for (auto it = users.begin(); it != users.end(); ++it)
            if (*it == user) {
                it = users.erase(it);
                if (it == users.end())return;
            }
    }

    bool resume(User* user_old, User* user_new, const std::string& password, std::string& info) {
        if (have_password && password != this->password) {
            info = "Password incorrect. ";
            return false;
        }
        for (auto it = users.begin(); it != users.end(); ++it)
            if (*it == user_old) {
                *it = user_new;
                return true;
            }
        return false;
    }

    bool everyone_is_prepared() {
        for (auto user : users)
            if (!user->prepared)
                return false;
        return true;
    }

    void start_uno_game() {
        std::vector<std::string> players_;
        players_.reserve(users.size());
        for (auto& user : users)
            players_.push_back(user->name);
        uno_game = std::make_unique<Uno>(players_);
        uno_game_started = true;
    }

    bool is_playing_game() {
        return uno_game_started;
    }

    int hash_card(Uno::Card card) {
        return card.content + card.color * 16;
    }
    Uno::Card hash_2_card(int card_hash) {
        return {
            static_cast<Uno::CardColor>(card_hash / 16),
            static_cast<Uno::CardContent>(card_hash % 16)
        };
    }

    Json::Value get_cards_in_hand(User* user) {
        const auto& player = *std::find_if(
            uno_game->players.begin(),
            uno_game->players.end(),
            [&user](const Uno::Player& p) {
                return p.name == user->name;
            }
        );
        Json::Value cards;
        cards.resize(0);
        for (auto card : player.cards_in_hand) {
            cards.append(Json::Value(hash_card(card)));
        }
        return cards;
    }

    int get_last_card() {
        return hash_card(uno_game->get_last_card());
    }

    int get_player_card_count(const std::string& player_name) {
        return uno_game->get_player_card_count(player_name);
    }

    Json::Value get_game_info() {
        Json::Value info;
        info["last_card"] = hash_card(uno_game->get_last_card());
        info["next_player"] = uno_game->get_next_player();
        info["specified_color"] = uno_game->get_specified_color();
        info["direction"] = uno_game->get_direction();
        info["players"].resize(0);
        for (auto& player : uno_game->players) {
            Json::Value p;
            p["name"] = player.name;
            p["count"] = player.cards_in_hand.size();
            info["players"].append(p);
        }
        return info;
    }

    std::string get_next_player() {
        return uno_game->get_next_player();
    }

    bool play(
        const std::string& player_name, 
        int card_hash, int specified_color,
        bool& punish
    ) {
        return uno_game->play(player_name, hash_2_card(card_hash),
            static_cast<Uno::CardColor>(specified_color), punish);
    }

    bool draw_one(const std::string& player_name, bool& punish, int& card) {
        Uno::Card c;
        bool res= uno_game->draw_one(player_name, punish, c);
        card = hash_card(c);
        return res;
    }

    bool skip_after_drawing_one(const std::string& player_name) {
        return uno_game->skip_after_drawing_one(player_name);
    }
    
    bool say_uno(const std::string& player_name) {
        return uno_game->say_uno(player_name);
    }

    std::vector<int> suspect(const std::string& player_name, bool& success, bool& valid, std::string& suspect_name) {
        auto cards = uno_game->suspect(player_name, success, valid, suspect_name);
        std::vector<int> cards_hash;
        cards_hash.reserve(cards.size());
        for (auto& card : cards)
            cards_hash.emplace_back(hash_card(card));
        return cards_hash;
    }

    bool dissuspect(const std::string& player_name) {
        return uno_game->dissuspect(player_name);
    }

    bool check_winner(std::string& winner) {
        return !(uno_game_started = !uno_game->check_winner(winner));
    }
};

#endif
