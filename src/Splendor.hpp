#ifndef SPLENDOR_HPP
#define SPLENDOR_HPP

#include <unordered_map>
#include <vector>
#include <array>
#include <string>
#include <ctime>
#include <cstdlib>
#include <json/json.h>

class Splendor {
public:
    enum Mine {
        COPPER, DIAMOND, EMERALD, IRON, NETHERITE, GOLD
    };

    struct Coupon {
        int                 reputation;
        std::array<int, 5>  costs{};        // use the Mine enum value as index
        Mine                type;
        int                 level;

        unsigned            idx;

       // inline static unsigned max_idx = 0;

        static Coupon empty(int level) {
            return Coupon{ -1, {}, GOLD, level };
        }

        bool is_empty() {
            return reputation == -1;
        }

        Json::Value to_json() {
            Json::Value obj;
            if (is_empty())
                obj["type"] = "EMPTY";
            else {
                obj["reputation"] = reputation;
                obj["costs"].resize(5);
                for (int i = 0; i < 5; ++i)
                    obj["costs"][i] = costs[i];
                obj["type"] = int(type);
                obj["level"] = level;
                obj["idx"] = idx;
            }
            return obj;
        }

        //static Coupon random_lv1() {
        //    std::array<int, 5>  costs{};
        //    int total = rand() % 2 + 3;
        //    int i = 0;
        //    while (total > 0 && i < 5) total -= (costs[i++] = rand() % 3);
        //    if (total > 0)costs[4] += total;
        //    return { rand() % 2, costs, static_cast<Mine>(rand() % 5), 1, max_idx++ };
        //}
        //
        //static Coupon random_lv2() {
        //    std::array<int, 5>  costs{};
        //    int total = rand() % 4 + 4;
        //    int i = 0;
        //    while (total > 0 && i < 5) total -= (costs[i++] = rand() % 3);
        //    if (total > 0)costs[4] += total;
        //    return { rand() % 4, costs, static_cast<Mine>(rand() % 5), 2, max_idx++ };
        //}
        //
        //static Coupon random_lv3() {
        //    std::array<int, 5>  costs{};
        //    int total = rand() % 5 + 6;
        //    int i = 0;
        //    while (total > 0 && i < 5) total -= (costs[i++] = rand() % 5);
        //    if (total > 0)costs[4] += total;
        //    return { rand() % 5, costs, static_cast<Mine>(rand() % 5), 3, max_idx++ };
        //}
    };

    struct Ally {
        int                 reputation;
        std::array<int, 5>  condition{};    // use the Mine enum value as index
        unsigned            idx;
        int                 owner_id = 0;
        bool                is_owned = false;

        Json::Value to_json() {
            Json::Value obj;
            obj["reputation"] = reputation;
            obj["condition"].resize(5);
            for (int i = 0; i < 5; ++i)
                obj["condition"][i] = condition[i];
            obj["idx"] = idx;
            obj["is_owned"] = is_owned;
            obj["owner_id"] = owner_id;
            return obj;
        }
    };

    struct Player {
        std::vector<Coupon> coupons;
        std::vector<Coupon> reserved_coupons;
        std::array<int, 5>  coupon_count{}; // use the Mine enum value as index
        std::array<int, 6>  mine_count{};   // use the Mine enum value as index
        //std::vector<Ally>   allies;
        int                 reputation = 0;

        enum Status {
            WAITING,
            ACTION,
            NEED_RETURN_MINE,
            CHOOSE_ALLY
        } status = WAITING;

        Json::Value to_json(int id) {
            Json::Value obj;
            obj["coupons"].resize(0);
            for (auto& c : coupons)
                obj["coupons"].append(c.to_json());
            obj["reserved_coupons"].resize(0);
            for (auto& c : reserved_coupons)
                obj["reserved_coupons"].append(c.to_json());
            obj["coupon_count"].resize(5);
            for (int i = 0; i < 5; ++i)
                obj["coupon_count"][i] = coupon_count[i];
            obj["mine_count"].resize(6);
            for (int i = 0; i < 6; ++i)
                obj["mine_count"][i] = mine_count[i];
            obj["allies"].resize(0);
            //for (auto& a : allies)
              //  obj["allies"].append(a.to_json());
            obj["reputation"] = reputation;
            obj["id"] = id;
            switch (status) {
            case Splendor::Player::WAITING:
                obj["status"] = "WAITING";
                break;
            case Splendor::Player::ACTION:
                obj["status"] = "ACTION";
                break;
            case Splendor::Player::NEED_RETURN_MINE:
                obj["status"] = "NEED_RETURN_MINE";
                break;
            case Splendor::Player::CHOOSE_ALLY:
                obj["status"] = "CHOOSE_ALLY";
                break;
            default:
                break;
            }
            return obj;
        }

        int total_mine_count() {
            int cnt = 0;
            for (int i = 0; i < 6; ++i)
                cnt += mine_count[i];
            return cnt;
        }
    };

private:
    std::array<int, 6> bank{};    // use the Mine enum value as index
    
    std::vector<Coupon> coupons;
    std::vector<Coupon> coupons_rest;

    std::vector<Ally> allies;

    std::unordered_map<int, Player> players;
    //std::unordered_map<std::string, Player>::iterator current_player;

public:
    Splendor(std::vector<int> player_ids) {
        srand(time(0));

        for (auto& p : player_ids)
            players.emplace(p, Player{});
        auto it = std::next(players.begin(), rand() % players.size());
        it->second.status = Player::ACTION;

        if (player_ids.size() == 2)
            bank.fill(4);
        else if (player_ids.size() == 3)
            bank.fill(5);
        else if (player_ids.size() == 4)
            bank.fill(7);
        bank[GOLD] = 5;
        int ally_count = player_ids.size() + 1;

        std::array<Ally, 10> all_allies {
            Ally{ 3,{0,0,0,4,4},0 },
            Ally{ 3,{0,4,4,0,0},1 },
            Ally{ 3,{3,3,3,0,0},2 },
            Ally{ 3,{0,3,3,3,0},3 },
            Ally{ 3,{3,0,0,3,3},4 },
            Ally{ 3,{3,0,3,0,3},5 },
            Ally{ 3,{4,0,4,0,0},6 },
            Ally{ 3,{4,0,0,0,4},7 },
            Ally{ 3,{0,4,0,4,0},8 },
            Ally{ 3,{0,3,0,3,3},9 }
        };

        allies.reserve(ally_count);
        for (int i = 9; i > 9 - ally_count; --i) {
            std::swap(all_allies[i], all_allies[rand() % i]);
            allies.push_back(all_allies[i]);
        }

        std::array<Coupon, 40> all_coupons_lv1 {
            Coupon{0, {0,0,0,3,0},COPPER,1,1},
            Coupon{0, {1,0,0,1,3},COPPER,1,2},
            Coupon{0, {0,2,1,0,0},COPPER,1,3},
            Coupon{0, {0,0,1,2,2},COPPER,1,4},
            Coupon{0, {0,1,1,2,1},COPPER,1,5},
            Coupon{0, {0,1,1,1,1},COPPER,1,6},
            Coupon{0, {2,0,0,2,0},COPPER,1,7},
            Coupon{1, {0,0,0,4,0},COPPER,1,8},
            Coupon{0, {0,0,0,1,2},DIAMOND,1,9},
            Coupon{0, {2,0,1,1,1},DIAMOND,1,10},
            Coupon{0, {1,0,1,1,1},DIAMOND,1,11},
            Coupon{0, {1,1,3,0,0},DIAMOND,1,12},
            Coupon{0, {0,0,0,0,3},DIAMOND,1,13},
            Coupon{0, {2,0,2,1,0},DIAMOND,1,14},
            Coupon{0, {0,0,2,0,2},DIAMOND,1,15},
            Coupon{1, {0,0,0,0,4},DIAMOND,1,16},
            Coupon{0, {0,1,0,2,0},EMERALD,1,17},
            Coupon{0, {2,2,0,0,0},EMERALD,1,18},
            Coupon{0, {0,3,1,1,0},EMERALD,1,19},
            Coupon{0, {1,1,0,1,1},EMERALD,1,20},
            Coupon{0, {1,1,0,1,2},EMERALD,1,21},
            Coupon{0, {2,1,0,0,2},EMERALD,1,22},
            Coupon{0, {3,0,0,0,0},EMERALD,1,23},
            Coupon{1, {0,0,0,0,4},EMERALD,1,24},
            Coupon{0, {0,2,2,0,1},IRON,1,25},
            Coupon{0, {2,0,0,0,1},IRON,1,26},
            Coupon{0, {1,1,1,0,1},IRON,1,27},
            Coupon{0, {0,3,0,0,0},IRON,1,28},
            Coupon{0, {0,2,0,0,2},IRON,1,29},
            Coupon{0, {1,1,2,0,1},IRON,1,30},
            Coupon{0, {0,1,0,0,1},IRON,1,31},
            Coupon{1, {0,0,4,0,0},IRON,1,32},
            Coupon{0, {1,1,1,1,0},NETHERITE,1,33},
            Coupon{0, {1,0,2,0,0},NETHERITE,1,34},
            Coupon{0, {0,0,2,2,0},NETHERITE,1,35},
            Coupon{0, {3,0,1,0,1},NETHERITE,1,36},
            Coupon{0, {0,0,3,0,0},NETHERITE,1,37},
            Coupon{0, {1,2,1,1,0},NETHERITE,1,38},
            Coupon{0, {1,2,0,1,0},NETHERITE,1,39},
            Coupon{1, {0,4,0,0,0},NETHERITE,1,40}
        };

        std::array<Coupon, 30> all_coupons_lv2 {
            Coupon{1, { 2,3,0,0,3 }, COPPER, 2, 41},
            Coupon{1, { 2,0,0,2,3 }, COPPER, 2, 42},
            Coupon{2, { 0,4,2,1,0 }, COPPER, 2, 43},
            Coupon{2, { 0,0,0,3,5 }, COPPER, 2, 44},
            Coupon{2, { 0,0,0,0,5 }, COPPER, 2, 45},
            Coupon{3, { 6,0,0,0,0 }, COPPER, 2, 46},
            Coupon{1, { 3,2,2,0,0 }, DIAMOND, 2, 47},
            Coupon{1, { 0,2,3,0,3 }, DIAMOND, 2, 48},
            Coupon{2, { 0,3,0,5,0 }, DIAMOND, 2, 49},
            Coupon{2, { 0,5,0,0,0 }, DIAMOND, 2, 50},
            Coupon{2, { 1,0,0,2,4 }, DIAMOND, 2, 51},
            Coupon{3, { 0,6,0,0,0 }, DIAMOND, 2, 52},
            Coupon{1, { 3,0,2,3,0 }, EMERALD, 2, 53},
            Coupon{1, { 0,3,0,3,2 }, EMERALD, 2, 54},
            Coupon{2, { 0,2,0,4,1 }, EMERALD, 2, 55},
            Coupon{2, { 0,0,5,0,0 }, EMERALD, 2, 56},
            Coupon{2, { 0,5,3,0,0 }, EMERALD, 2, 57},
            Coupon{3, { 0,0,6,0,0 }, EMERALD, 2, 58},
            Coupon{1, { 2,0,3,0,2 }, IRON, 2, 59},
            Coupon{1, { 3,3,0,2,0 }, IRON, 2, 60},
            Coupon{2, { 4,0,1,0,2 }, IRON, 2, 61},
            Coupon{2, { 5,0,0,0,0 }, IRON, 2, 62},
            Coupon{2, { 5,0,0,0,3 }, IRON, 2, 63},
            Coupon{3, { 0,0,0,6,0 }, IRON, 2, 64},
            Coupon{1, { 0,2,2,3,0 }, NETHERITE, 2, 65},
            Coupon{1, { 0,0,3,3,2 }, NETHERITE, 2, 66},
            Coupon{2, { 2,1,4,0,0 }, NETHERITE, 2, 67},
            Coupon{2, { 0,0,0,5,0 }, NETHERITE, 2, 68},
            Coupon{2, { 3,0,5,0,0 }, NETHERITE, 2, 69},
            Coupon{3, { 0,0,0,0,6 }, NETHERITE, 2, 70}
        };

        std::array<Coupon, 20> all_coupons_lv3 {
            Coupon{3, { 0,5,3,3,3 }, COPPER, 3, 71},
            Coupon{4, { 0,0,7,0,0 }, COPPER, 3, 72},
            Coupon{4, { 3,3,6,0,0 }, COPPER, 3, 73},
            Coupon{5, { 3,0,7,0,0 }, COPPER, 3, 74},
            Coupon{3, { 3,0,3,3,5 }, DIAMOND, 3, 75},
            Coupon{4, { 0,0,0,7,0 }, DIAMOND, 3, 76},
            Coupon{4, { 0,3,0,6,3 }, DIAMOND, 3, 77},
            Coupon{5, { 0,3,0,7,0 }, DIAMOND, 3, 78},
            Coupon{3, { 3,3,0,5,3 }, EMERALD, 3, 79},
            Coupon{4, { 0,6,3,3,0 }, EMERALD, 3, 80},
            Coupon{4, { 0,7,0,0,0 }, EMERALD, 3, 81},
            Coupon{5, { 0,7,3,0,0 }, EMERALD, 3, 82},
            Coupon{3, { 5,3,3,0,3 }, IRON, 3, 83},
            Coupon{4, { 0,0,0,0,7 }, IRON, 3, 84},
            Coupon{4, { 3,0,0,3,6 }, IRON, 3, 85},
            Coupon{5, { 0,0,0,3,7 }, IRON, 3, 86},
            Coupon{3, { 3,3,5,3,0 }, NETHERITE, 3, 87},
            Coupon{4, { 7,0,0,0,0 }, NETHERITE, 3, 88},
            Coupon{4, { 6,0,3,0,3 }, NETHERITE, 3, 89},
            Coupon{5, { 7,0,0,0,3 }, NETHERITE, 3, 90}
        };

        for (int i = 39; i > 0; --i)
            std::swap(all_coupons_lv1[i], all_coupons_lv1[rand() % i]);
        for (int i = 29; i > 0; --i)
            std::swap(all_coupons_lv2[i], all_coupons_lv2[rand() % i]);
        for (int i = 19; i > 0; --i)
            std::swap(all_coupons_lv3[i], all_coupons_lv3[rand() % i]);

        coupons_rest.insert(coupons_rest.end(), all_coupons_lv1.begin(), all_coupons_lv1.begin() + 36);
        coupons_rest.insert(coupons_rest.end(), all_coupons_lv2.begin(), all_coupons_lv2.begin() + 26);
        coupons_rest.insert(coupons_rest.end(), all_coupons_lv3.begin(), all_coupons_lv3.begin() + 16);

        coupons.insert(coupons.end(), all_coupons_lv1.begin() + 36, all_coupons_lv1.end());
        coupons.insert(coupons.end(), all_coupons_lv2.begin() + 26, all_coupons_lv2.end());
        coupons.insert(coupons.end(), all_coupons_lv3.begin() + 16, all_coupons_lv3.end());

        //for (int i = 0; i < 36; ++i)
        //    coupons_rest.push_back(Coupon::random_lv1());
        //for (int i = 0; i < 4; ++i)
        //    coupons.push_back(Coupon::random_lv1());
        //for (int i = 0; i < 26; ++i)
        //    coupons_rest.push_back(Coupon::random_lv2());
        //for (int i = 0; i < 4; ++i)
        //    coupons.push_back(Coupon::random_lv2());
        //for (int i = 0; i < 16; ++i)
        //    coupons_rest.push_back(Coupon::random_lv3());
        //for (int i = 0; i < 4; ++i)
        //    coupons.push_back(Coupon::random_lv3());
    }

    Json::Value get_game_info() {
        Json::Value info;
        info["allies"].resize(0);
        for (auto& a : allies) 
            info["allies"].append(a.to_json());

        info["coupon_lv1"].resize(0);
        info["coupon_lv2"].resize(0);
        info["coupon_lv3"].resize(0);
        for(auto& c: coupons)
            info["coupon_lv" + std::to_string(c.level)].append(c.to_json());

        info["bank"].resize(6);
        for (int i = 0; i < 6; ++i)
            info["bank"][i] = (bank[i]);

        info["players"].resize(0);
        for (auto& p : players)
            info["players"].append(p.second.to_json(p.first));

        return info;
    }

    Json::Value get_player_info(int player) {
        auto it = players.find(player);
        if (it == players.end())
            return {};
        return it->second.to_json(player);
    }

    bool take_3_mines(const std::array<Mine, 3> mines, int player) {
        auto it = players.find(player);
        if (it == players.end())
            return false;
        auto& p = it->second;
        if (p.status != Player::ACTION)
            return false;
        if (mines[0] == mines[1]
            || mines[1] == mines[2]
            || mines[2] == mines[0])
            return false;
        if (bank[mines[0]] == 0
            || bank[mines[1]] == 0
            || bank[mines[2]] == 0)
            return false;
        if (mines[0] == GOLD
            || mines[1] == GOLD
            || mines[2] == GOLD)
            return false;

        bank[mines[0]]--;
        bank[mines[1]]--;
        bank[mines[2]]--;
        p.mine_count[mines[0]]++;
        p.mine_count[mines[1]]++;
        p.mine_count[mines[2]]++;

        if (p.total_mine_count() > 10)
            p.status = Player::NEED_RETURN_MINE;
        else {
            p.status = Player::WAITING;

            std::advance(it, 1);
            if (it == players.end())
                it = players.begin();
            it->second.status = Player::ACTION;
        }

        return true;
    }

    bool take_2_mines(Mine mine, int player) {
        auto it = players.find(player);
        if (it == players.end())
            return false;
        auto& p = it->second;
        if (p.status != Player::ACTION)
            return false;
        if (bank[mine] < 4)
            return false;
        if (mine == GOLD)
            return false;

        bank[mine] -= 2;
        p.mine_count[mine] += 2;

        if (p.total_mine_count() > 10)
            p.status = Player::NEED_RETURN_MINE;
        else {
            p.status = Player::WAITING;

            std::advance(it, 1);
            if (it == players.end())
                it = players.begin();
            it->second.status = Player::ACTION;
        }

        return true;
    }

    bool reserve_coupon(int coupon_idx, int player) {
        auto it = players.find(player);
        if (it == players.end())
            return false;
        auto& p = it->second;
        if (p.status != Player::ACTION)
            return false;
        if (p.reserved_coupons.size() > 2)
            return false;
        auto coupon_it = std::find_if(
            coupons.begin(), coupons.end(),
            [&](const Coupon& c) {
                return c.idx == coupon_idx;
            }
        );
        if (coupon_it == coupons.end())
            return false;

        p.reserved_coupons.push_back(*coupon_it);
        auto level = coupon_it->level;
        //coupons.erase(coupon_it);
        fill_coupon(level, *coupon_it);

        if (bank[GOLD] > 0) {
            p.mine_count[GOLD]++;
            bank[GOLD]--;
        }

        if (p.total_mine_count() > 10)
            p.status = Player::NEED_RETURN_MINE;
        else {
            p.status = Player::WAITING;

            std::advance(it, 1);
            if (it == players.end())
                it = players.begin();
            it->second.status = Player::ACTION;
        }

        return true;
    }

    bool buy_coupon(int coupon_idx, int player) {
        auto it = players.find(player);
        if (it == players.end())
            return false;
        auto& p = it->second;
        if (p.status != Player::ACTION)
            return false;
        auto coupon_it = std::find_if(
            coupons.begin(), coupons.end(),
            [&](const Coupon& c) {
                return c.idx == coupon_idx;
            }
        );
        if (coupon_it == coupons.end()) 
            return false;
        if (!check_affordabillity(p, *coupon_it))
            return false;

        for (int mine = 0; mine < 5; ++mine) {
            int cost = coupon_it->costs[mine] - p.coupon_count[mine];
            if (cost > 0) {
                p.mine_count[mine] -= cost;
                bank[mine] += cost;
                if (p.mine_count[mine] < 0) {
                    p.mine_count[GOLD] += p.mine_count[mine];
                    bank[mine] += p.mine_count[mine];
                    bank[GOLD] -= p.mine_count[mine];
                    p.mine_count[mine] = 0;
                }
            }
        }

        p.coupons.push_back(*coupon_it);
        p.coupon_count[coupon_it->type]++;
        p.reputation += coupon_it->reputation;
        auto level = coupon_it->level;
//        coupons.erase(coupon_it);
        fill_coupon(level, *coupon_it);

        p.status = Player::WAITING;
        std::advance(it, 1);
        if (it == players.end())
            it = players.begin();
        it->second.status = Player::ACTION;

        check_ally();

        return true;
    }

private:
    void fill_coupon(int level, Coupon& old_coupon) {
        auto new_coupon = std::find_if(
            coupons_rest.begin(), coupons_rest.end(),
            [&](const Coupon& c) {
                return c.level == level;
            }
        );
        if (new_coupon != coupons_rest.end()) {
            old_coupon = *new_coupon;
            //coupons.push_back(*new_coupon);
            coupons_rest.erase(new_coupon);
        }
        else
            old_coupon = Coupon::empty(level);
    }



public:
    bool buy_reserved_coupon(int coupon_idx, int player) {
        auto it = players.find(player);
        if (it == players.end())
            return false;
        auto& p = it->second;
        if (p.status != Player::ACTION)
            return false;
        auto coupon_it = std::find_if(
            p.reserved_coupons.begin(), p.reserved_coupons.end(),
            [&](const Coupon& c) {
                return c.idx == coupon_idx;
            }
        );
        if (coupon_it == p.reserved_coupons.end())
            return false;
        if(!check_affordabillity(p, *coupon_it))
            return false;
        
        for (int mine = 0; mine < 5; ++mine) {
            int cost = coupon_it->costs[mine] - p.coupon_count[mine];
            if (cost > 0) {
                p.mine_count[mine] -= cost;
                bank[mine] += cost;
                if (p.mine_count[mine] < 0) {
                    p.mine_count[GOLD] += p.mine_count[mine];
                    bank[mine] += p.mine_count[mine];
                    bank[GOLD] -= p.mine_count[mine];
                    p.mine_count[mine] = 0;
                }
            }
        }

        p.coupons.push_back(*coupon_it);
        assert(coupon_it->type != GOLD);
        p.coupon_count[coupon_it->type]++;
        p.reputation += coupon_it->reputation;
        p.reserved_coupons.erase(coupon_it);

        p.status = Player::WAITING;
        std::advance(it, 1);
        if (it == players.end())
            it = players.begin();
        it->second.status = Player::ACTION;

        check_ally();
        return true;
    }

private:
    bool check_affordabillity(const Player& p, const Coupon& c) {
        int gold_cnt = p.mine_count[GOLD];
        for (int mine = 0; mine < 5; ++mine)
            if (p.mine_count[mine] + p.coupon_count[mine] + gold_cnt < c.costs[mine])
                return false;
            else gold_cnt -=
                [](int x) { return x > 0 ? x : 0; }(
                    (c.costs[mine] - p.mine_count[mine] - p.coupon_count[mine])
                    );
        return true;
    }

public:
    bool return_mine(Mine mine, int player) {
        auto it = players.find(player);
        if (it == players.end())
            return false;
        auto& p = it->second;

        if (p.status != Player::NEED_RETURN_MINE)
            return false;

        if (p.mine_count[mine] < 1)
            return false;

        p.mine_count[mine]--;
        bank[mine]++;

        if (p.total_mine_count() < 11) {
            p.status = Player::WAITING;
            std::advance(it, 1);
            if (it == players.end())
                it = players.begin();
            it->second.status = Player::ACTION;
        }

        return true;
    }

    void check_ally() {
        for (auto& a : allies) 
            if(!a.is_owned)
                for (auto& p : players) {
                    bool ok = true;
                    for (int c = 0; c < 5; ++c)
                        if (p.second.coupon_count[c] < a.condition[c]) {
                            ok = false;
                            break;
                        }
                    if (ok) {
                        a.is_owned = true;
                        a.owner_id = p.first;
                        p.second.reputation += a.reputation;
                    }
                }
    }

    bool check_winner(int& winner) {
        for(auto& p:players)
            if (p.second.reputation > 14) {
                winner = p.first;
                return true;
            }
        return false;
    }
};

#endif
