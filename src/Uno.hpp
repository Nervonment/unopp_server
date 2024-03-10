#ifndef UNO_HPP
#define UNO_HPP

#include <vector>
#include <queue>
#include <string>
#include <cstdlib>
#include <ctime>

class Uno {
public:
    enum CardContent {
        // numbers
        ZERO,ONE,TWO,THREE,FOUR,FIVE,SIX,SEVEN,EIGHT,NINE,

        // functions
        SKIP, DRAW_2,REVERSE,

        // wild
        WILD, WILD_DRAW_4 
    };
    enum CardColor {
        RED,YELLOW,GREEN, BLUE,
        BLACK
    };
    struct Card {
        CardColor color;
        CardContent content;

        bool operator==(Card another) const {
            return color == another.color &&
                content == another.content;
        }
        bool operator!=(Card another) const {
            return color != another.color ||
                content != another.content;
        }
    };

    struct Player {
        std::string user_name;
        std::vector<Card> cards_in_hand;
        std::vector<Card> cards_when_wild_draw_4;
        bool drawn_one = false;
        Card last_drew;
        bool said_uno = false;
    };

private:
    std::vector<Player> players_;
    std::vector<Card> all_cards;
    std::queue<Card> deck;
    CardColor last_color;
    CardContent last_number;
    Card card_b4_wild_draw_4;
    bool reversed = false;
    bool wait_suspect = false;
    int next_player_idx = 0;

public:
    const std::vector<Player>& players = players_;
    
public:
    Uno(const std::vector<std::string> & players_) {
        this->players_.reserve(players_.size());
        for (auto& user_name : players_)
            this->players_.emplace_back(Player{ user_name,{} });

        srand(time(0));

        all_cards.clear();
        all_cards.reserve(108);
        const CardColor all_colors[] = {
            RED,YELLOW,GREEN, BLUE
        };
        const CardContent all_contents[] = {
            ONE,TWO,THREE,FOUR,FIVE,SIX,SEVEN,EIGHT,NINE,
            SKIP, DRAW_2,REVERSE,
        };
        for (auto color : all_colors)
            for (auto content : all_contents) {
                all_cards.emplace_back(Card{ color, content });
                all_cards.emplace_back(Card{ color, content });
            }
        for (auto color : all_colors)
            all_cards.emplace_back(Card{ color, ZERO });
        for (int i = 0; i < 4; ++i) {
            all_cards.emplace_back(Card{ BLACK, WILD });
            all_cards.emplace_back(Card{ BLACK, WILD_DRAW_4 });
        }

        init();
    }

public:
    void init() {
        for (int i = all_cards.size() - 1; i > 0; --i)
            std::swap(all_cards[i], all_cards[rand() % i]);
        while (!deck.empty())
            deck.pop();
        for (auto& card : all_cards)
            deck.push(card);

        next_player_idx = rand() % players_.size();

        for (auto& player : players_) {
            player.cards_in_hand.clear();
            player.drawn_one = false;
            player.said_uno = false;
            player.cards_in_hand.reserve(15);
            for (int i = 0; i < 7; ++i) {
                player.cards_in_hand.push_back(deck.front());
                deck.pop();
            }
        }

        while (!is_number_card(get_last_card())) {
            deck.push(deck.front());
            deck.pop();
        }

        last_color = get_last_card().color;
        last_number = get_last_card().content;
        reversed = false;
    }

    bool is_number_card(Card card) {
        return card.content <= 10;
    }

    Card give(Player& player, int count) {
        Card res;
        for (int i = 0; i < count; ++i) {
            player.cards_in_hand.push_back(res = deck.front());
            deck.pop();
        }
        return res;
    }

public:
    Card get_last_card() {
        return deck.back();
    }
    std::string get_next_player() {
        return players_[next_player_idx].user_name;
    }
    CardColor get_specified_color(){
        return last_color;
    }
    bool get_direction() {
        return reversed;
    }
    int get_player_card_count(const std::string& player_name) {
        for (auto& player : players_)
            if (player.user_name == player_name) 
                return player.cards_in_hand.size();
    }

    void next() {
        reversed ? next_player_idx++ : next_player_idx--;
        (next_player_idx += players_.size()) %= players_.size();
    }

    bool play(
        const std::string& player_name,
        Card card, CardColor specified_color,
        bool& punish
    ) {
        punish = false;
        if (wait_suspect)
            return false;

        bool ok =
            card.color == BLACK ? true :
            //is_number_card(card) ?
            //card.color == last_color || card.content == last_number :
            //card.color == last_color;
            card.color == last_color || card.content == last_number;

        if (!ok)
            return false;

        auto& player = players_[next_player_idx];
        if (player_name != player.user_name)
            return false;

        if (player.drawn_one && player.last_drew != card)
            return false;
        player.drawn_one = false;

        if (card.content == WILD_DRAW_4) 
            player.cards_when_wild_draw_4 = player.cards_in_hand;

        deck.push(card);
        for (auto it = player.cards_in_hand.begin();
            it != player.cards_in_hand.end(); ++it)
            if (*it == card) {
                player.cards_in_hand.erase(it);
                break;
            }

        // punish
        if (player.cards_in_hand.size() == 1 && !player.said_uno) {
            punish = true;
            give(player, 2);
        }
        else punish = false;
        player.said_uno = false;


        if (card.content == REVERSE)
            reversed = !reversed;

        next();
        
        if (card.content == DRAW_2) {
            give(players_[next_player_idx], 2);
            next();
        }

        else if (card.content == SKIP)
            next();

        else if (card.content == WILD_DRAW_4) {
            wait_suspect = true;
            card_b4_wild_draw_4 = { last_color, last_number };
        }

        // specify a color
        if (card.color == BLACK)
            last_color = specified_color;
        else last_color = card.color;
        last_number = card.content;

        return true;
    }

    bool draw_one(const std::string& player_name, bool& punish, Card& card) {
        punish = false;

        if (wait_suspect)
            return false;
        
        auto& player = players_[next_player_idx];
        if (player_name != player.user_name)
            return false;
        if (player.drawn_one)
            return false;
        if (player.said_uno) {
            punish = true;
            give(player, 2);
            player.said_uno = false;
        }

        player.drawn_one = true;
        card = player.last_drew = give(player, 1);
        return true;
    }

    bool skip_after_drawing_one(const std::string& player_name) {
        if (wait_suspect)
            return false;
        
        auto& player = players_[next_player_idx];
        if (player_name != player.user_name)
            return false;

        if (!player.drawn_one)
            return false;

        player.drawn_one = false;
        next();
        return true;
    }

    bool say_uno(const std::string& player_name) {
        auto& player = players_[next_player_idx];
        if (player_name != player.user_name) 
            for(auto& player: players_)
                if (player.user_name == player_name) {
                    give(player, 2);
                    return false;
                }
        if(player.cards_in_hand.size() != 2) {
            give(player, 2);
            return false;
        }
        player.said_uno = true;
        return true;
    }

    std::vector<Card> suspect(const std::string& player_name, bool& success, bool& valid, std::string& suspect_name) {
        valid = true;
        if (!wait_suspect) {
            valid = false;
            return {};
        }
        auto& player = players_[next_player_idx];
        auto& sus = get_last_player_ref();
        if (player_name != player.user_name) {
            valid = false;
            return {};
        }
        success = false;
        for (auto& card : sus.cards_when_wild_draw_4)
            if (card.color == card_b4_wild_draw_4.color ||
                (card_b4_wild_draw_4.content != WILD &&
                    card_b4_wild_draw_4.content != WILD_DRAW_4 &&
                    card.content == card_b4_wild_draw_4.content)
                ) {
                success = true; break;
            }
        if (success)
            give(sus, 4);
        else {
            give(player, 6);
            next();
        }
        wait_suspect = false;
        suspect_name = sus.user_name;
        return sus.cards_in_hand;
    }

    bool dissuspect(const std::string& player_name) {
        if (!wait_suspect)
            return false;
        auto& player = players_[next_player_idx];
        if (player_name != player.user_name) 
            return false;
        give(player, 4);
        next();
        wait_suspect = false;
        return true;
    }

private:
    Player& get_last_player_ref() {
        return players_[
            ((reversed ? next_player_idx - 1 : next_player_idx + 1) + players_.size()) % players_.size()
        ];
    }

public:
    bool check_winner(std::string& winner) {
        for (auto& player : players) 
            if (player.cards_in_hand.empty()) {
                winner = player.user_name;
                return true;
            }
        return false;
    }
};

#endif