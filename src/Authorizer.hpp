#ifndef AUTHORIZER_HPP
#define AUTHORIZER_HPP

#include <iostream>
#include <random>
#include <mutex>
#include <fstream>

#include <sqlite3.h>
#include <SQLiteCpp/SQLiteCpp.h>
#include <json/json.h>

class Authorizer {
public:
    enum class Result {
        SUCCESS,
        FAILED,                 // unknown reason

        USERNAME_DUPLICATE,
        USERNAME_INVALID,
        PASSWORD_EMPTY,

        USER_DONOT_EXIST,
        PASSWORD_INCORRECT,

        SESSDATA_INVALID,

        SET_ICON_FAILED,

        ALREADY_REQUESTED,
        ALREADY_FRIEND,
        CANNOT_REQUEST_SELF,
    };

private:
    SQLite::Database db{ "users.db", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE };
    std::mutex db_mutex;

    std::unordered_multimap<int, std::pair<int, int>> cache;
    std::mutex cache_mutex;

    Authorizer() {
        std::thread t(std::bind(&Authorizer::write_cache_to_database, this));
        t.detach();
    }
public:
    static Authorizer& get_instance() {
        static Authorizer auth;
        return auth;
    }

    Result new_user(const std::string& user_name, const std::string& password) {
        if (user_name.empty() || user_name.size() > 40)
            return Result::USERNAME_INVALID;
        if (password.empty())
            return Result::PASSWORD_EMPTY;

        {
            std::lock_guard<std::mutex> lock(db_mutex);
            SQLite::Statement query_dup(db, "SELECT * FROM user WHERE user_name=?");
            query_dup.bind(1, user_name);
            if (query_dup.executeStep())
                return Result::USERNAME_DUPLICATE;

            SQLite::Transaction tr(db);
            SQLite::Statement insert(db, "INSERT INTO user (user_name, password) VALUES (?, ?)");
            insert.bind(1, user_name);
            insert.bind(2, password);
            insert.exec();
            tr.commit();
        }

        return Result::SUCCESS;
    }

    unsigned generate_sessdata(const std::string& user_name) {
        time_t t;
        time(&t);
        std::mt19937 mt_rand(std::random_device{}());
        return (static_cast<unsigned int>(mt_rand() + std::hash<std::string>()(user_name)) << 16) + t;
    }

    Result log_in(const std::string& user_name, const std::string& password, int& id, unsigned int& sessdata) {
        {
            std::lock_guard<std::mutex> lock(db_mutex);
            SQLite::Statement query_exist(db, "SELECT id, password FROM user WHERE user_name=?");
            query_exist.bind(1, user_name);
            if (!query_exist.executeStep())
                return Result::USER_DONOT_EXIST;

            id = query_exist.getColumn(0);
            std::string password_correct = query_exist.getColumn(1);
            if (password_correct != password)
                return Result::PASSWORD_INCORRECT;
        }

        sessdata = generate_sessdata(user_name);
        {
            std::lock_guard<std::mutex> lock(db_mutex);
            SQLite::Transaction tr(db);
            SQLite::Statement insert(db, "UPDATE user SET sessdata=? WHERE id=?");
            insert.bind(1, sessdata);
            insert.bind(2, id);
            insert.exec();
            tr.commit();
        };

        return Result::SUCCESS;
    }

    Result log_in(int id, const std::string& password, std::string& user_name, unsigned int& sessdata) {
        {
            std::lock_guard<std::mutex> lock(db_mutex);
            SQLite::Statement query_exist(db, "SELECT user_name, password FROM user WHERE id=?");
            query_exist.bind(1, id);
            if (!query_exist.executeStep())
                return Result::USER_DONOT_EXIST;

            std::string name = query_exist.getColumn(0);
            user_name = name;
            std::string password_correct = query_exist.getColumn(1);
            if (password_correct != password)
                return Result::PASSWORD_INCORRECT;
        }

        sessdata = generate_sessdata(user_name);
        {
            std::lock_guard<std::mutex> lock(db_mutex);
            SQLite::Transaction tr(db);
            SQLite::Statement insert(db, "UPDATE user SET sessdata=? WHERE id=?");
            insert.bind(1, sessdata);
            insert.bind(2, id);
            insert.exec();
            tr.commit();
        };

        return Result::SUCCESS;
    }

    Result log_out(unsigned int sessdata) {
        {
            std::lock_guard<std::mutex> lock(db_mutex);
            SQLite::Statement query_exist(db, "SELECT id FROM user WHERE sessdata=?");
            query_exist.bind(1, sessdata);
            if (!query_exist.executeStep())
                return Result::USER_DONOT_EXIST;

            int id = query_exist.getColumn(0);

            SQLite::Transaction tr(db);
            SQLite::Statement insert(db, "UPDATE user SET sessdata=NULL WHERE id=?");
            insert.bind(1, id);
            insert.exec();
            tr.commit();
        }

        return Result::SUCCESS;
    }

    Result authorize(const unsigned int& sessdata, int& id, std::string& user_name) {
        {
            std::lock_guard<std::mutex> lock(db_mutex);
            SQLite::Statement query_exist(db, "SELECT id, user_name FROM user WHERE sessdata=?");
            query_exist.bind(1, sessdata);
            if (!query_exist.executeStep())
                return Result::SESSDATA_INVALID;

            id = query_exist.getColumn(0);
            std::string name = query_exist.getColumn(1);
            user_name = name;
        }

        return Result::SUCCESS;
    }

    // Deprecated
    Result set_icon(int id, const std::string& icon = "") {
        {
            std::lock_guard<std::mutex> lock(db_mutex);
            SQLite::Transaction tr(db);
            SQLite::Statement modify(db, "UPDATE user SET icon=? WHERE id=?");
            modify.bind(1, icon);
            modify.bind(2, id);
            modify.exec();
            tr.commit();
        }

        return Result::SUCCESS;
    }

    Result set_icon_new(int id, const char* buffer, size_t len) {
        std::ofstream fout("./icons/" + std::to_string(id) + ".png", std::ios::binary);
        if (!fout)
            return Result::SET_ICON_FAILED;
        fout.write(buffer, len);
        fout.close();
        return Result::SUCCESS;
    }

    // Deprecated
    Result get_icon(int id, std::string& icon) {
        {
            std::lock_guard<std::mutex> lock(db_mutex);
            SQLite::Statement query(db, "SELECT icon FROM user WHERE id=?");
            query.bind(1, id);
            query.executeStep();
            std::string ico = query.getColumn(0);
            icon = ico;
        }

        return Result::SUCCESS;
    }

    Result get_icon(const std::string& user_name, std::string& icon) {
        {
            std::lock_guard<std::mutex> lock(db_mutex);
            SQLite::Statement query(db, "SELECT icon FROM user WHERE user_name=?");
            query.bind(1, user_name);
            query.executeStep();
            std::string ico = query.getColumn(0);
            icon = ico;
        }

        return Result::SUCCESS;
    }

    Result set_user_name(int id, const std::string& new_user_name) {
        {
            std::lock_guard<std::mutex> lock(db_mutex);
            SQLite::Statement query_dup(db, "SELECT * FROM user WHERE user_name=?");
            query_dup.bind(1, new_user_name);
            if (query_dup.executeStep())
                return Result::USERNAME_DUPLICATE;

            SQLite::Transaction tr(db);
            SQLite::Statement modify(db, "UPDATE user SET user_name=? WHERE id=?");
            modify.bind(1, new_user_name);
            modify.bind(2, id);
            modify.exec();
            tr.commit();
        }

        return Result::SUCCESS;
    }

    Result raise_friend_request(int requester_id, int requestee_id) {
        if (requester_id == requestee_id)
            return Result::CANNOT_REQUEST_SELF;
        
        {
            std::lock_guard<std::mutex> lock(db_mutex);

            SQLite::Statement q1(db, "SELECT 1 FROM user WHERE id=?");
            q1.bind(1, requester_id);
            if (!q1.executeStep())
                return Result::USER_DONOT_EXIST;

            SQLite::Statement q2(db, "SELECT 1 FROM user WHERE id=?");
            q2.bind(1, requestee_id);
            if (!q2.executeStep())
                return Result::USER_DONOT_EXIST;

            SQLite::Statement q3(db, "SELECT 1 FROM relation WHERE user_id=? AND friend_id=?");
            q3.bind(1, requester_id);
            q3.bind(2, requestee_id);
            if (q3.executeStep())
                return Result::ALREADY_FRIEND;

            SQLite::Transaction tr(db);
            SQLite::Statement query(db, "INSERT INTO friend_request (requester_id, requestee_id) VALUES (?, ?)");
            query.bind(1, requester_id);
            query.bind(2, requestee_id);
            try {
                query.executeStep();
            }
            catch (const std::exception&) {
                return Result::ALREADY_REQUESTED;
            }
            tr.commit();
        }

        return Result::SUCCESS;
    }

    Result get_friend_requests(int id, Json::Value& requests) {
        {
            std::lock_guard<std::mutex> lock(db_mutex);

            SQLite::Statement q1(db, "SELECT requester_id FROM friend_request WHERE requestee_id=?");
            q1.bind(1, id);
            requests.resize(0);
            
            try {
                while (q1.executeStep()) {
                    int requester = q1.getColumn(0);
                    requests.append(query_for_user_info(requester));
                    //SQLite::Statement q2(db, "SELECT user_name FROM user WHERE id=?");
                    //q2.bind(1, requester);
                    //q2.executeStep();
                    //std::string name = q2.getColumn(0);
                    //Json::Value v;
                    //v["name"] = name;
                    //v["id"] = requester;
                    //requests.append(v);
                }
            }
            catch (const std::exception&) {
                return Result::FAILED;
            }
        }

        return Result::SUCCESS;
    }

    Result remove_friend_request(int id, int requester_id) {
        {
            std::lock_guard<std::mutex> lock(db_mutex);

            SQLite::Statement q1(db, "DELETE FROM friend_request WHERE requester_id=? AND requestee_id=?");
            q1.bind(1, requester_id);
            q1.bind(2, id);

            try {
                SQLite::Transaction tr(db);
                q1.executeStep();
                q1.reset();
                q1.bind(1, id);
                q1.bind(2, requester_id);
                q1.executeStep();
                tr.commit();
            }
            catch (const std::exception&) {
                return Result::FAILED;
            }

            return Result::SUCCESS;
        }
    }

    Result accept_friend_request(int id, int requester_id) {
        if (remove_friend_request(id, requester_id) == Result::FAILED)
            return Result::FAILED;
        {
            try {
                std::lock_guard<std::mutex> lock(db_mutex);

                SQLite::Transaction tr(db);
                SQLite::Statement q1(db, "INSERT INTO relation (user_id, friend_id) VALUES (?,?)");
                q1.bind(1, id);
                q1.bind(2, requester_id);
                q1.executeStep();

                q1.reset();
                q1.bind(1, requester_id);
                q1.bind(2, id);
                q1.executeStep();
                tr.commit();
            }
            catch (const std::exception&) {
                return Result::FAILED;
            }
        }

        return Result::SUCCESS;
    }

    Json::Value get_friend_list(int id) {
        Json::Value res;
        res.resize(0);
        {
            std::lock_guard<std::mutex> lock(db_mutex);
            std::lock_guard<std::mutex> lock1(cache_mutex);
            SQLite::Statement q1(db, "SELECT friend_id, unread FROM relation WHERE user_id=?");
            q1.bind(1, id);
            while (q1.executeStep()) {
                int friend_id = q1.getColumn(0);
                int unread = q1.getColumn(1);
                auto [begin, end] = cache.equal_range(id);
                for (auto it = begin; it != end; ++it)
                    if (it->second.first == friend_id) {
                        unread += it->second.second;
                        break;
                    }
                auto f = query_for_user_info(friend_id);
                f["unread"] = unread;
                res.append(std::move(f));
                //SQLite::Statement q2(db, "SELECT user_name, slogan FROM user WHERE id=?");
                //q2.bind(1, friend_id);
                //q2.executeStep();
                //Json::Value f;
                //f["name"] = q2.getColumn(0).getString();
                //f["id"] = friend_id;
                //f["slogan"] = q2.getColumn(1).getString();
                //res.append(f);
            }
        }
        return res;
    }

    Json::Value get_user_info(int id) {
        Json::Value res;

        {
            std::lock_guard<std::mutex> lock(db_mutex);
            res = query_for_user_info(id);
        }
        return res;
    }

private:
    Json::Value query_for_user_info(int id) {
        Json::Value res;
        SQLite::Statement q1(db, "SELECT user_name, slogan FROM user WHERE id=?");
        q1.bind(1, id);
        if (q1.executeStep()) {
            res["name"] = q1.getColumn(0).getString();
            res["slogan"] = q1.getColumn(1).getString();
            res["id"] = id;
        }
        return res;
    }

public:
    Result set_slogan(int id, const std::string& slogan) {
        {
            std::lock_guard<std::mutex> lock(db_mutex);
            SQLite::Statement q1(db, "UPDATE user SET slogan=? WHERE id=?");
            q1.bind(1, slogan);
            q1.bind(2, id);
            SQLite::Transaction tr(db);
            q1.executeStep();
            tr.commit();
        }
        return Result::SUCCESS;
    }

    Result add_one_unread(int user_id, int friend_id) {
        {
            std::lock_guard<std::mutex> lock(cache_mutex);
            auto [begin, end] = cache.equal_range(user_id);
            bool found = false;
            for (auto it = begin; it != end; ++it)
                if (it->second.first == friend_id) {
                    it->second.second++;
                    found = true;
                    break;
                }
            if (!found)
                cache.emplace(user_id, std::pair{ friend_id, 1 });
        }
        //{
        //    std::lock_guard<std::mutex> lock(db_mutex);
        //    SQLite::Statement q1(db, "UPDATE relation SET unread=unread+1 WHERE user_id=? AND friend_id=?");
        //    q1.bind(1, user_id);
        //    q1.bind(2, friend_id);
        //    SQLite::Transaction tr(db);
        //    q1.executeStep();
        //    tr.commit();
        //}
        return Result::SUCCESS;
    }

    Result clear_unread(int user_id, int friend_id) {
        {
            std::lock_guard<std::mutex> lock(cache_mutex);
            auto [begin, end] = cache.equal_range(user_id);
            for (auto it = begin; it != end; ++it)
                if (it->second.first == friend_id) {
                    it->second.second = 0;
                    break;
                }
        }
        {
            std::lock_guard<std::mutex> lock(db_mutex);
            SQLite::Statement q1(db, "UPDATE relation SET unread=0 WHERE user_id=? AND friend_id=?");
            q1.bind(1, user_id);
            q1.bind(2, friend_id);
            SQLite::Transaction tr(db);
            q1.executeStep();
            tr.commit();
        }
        return Result::SUCCESS;
    }

private:
    void write_cache_to_database(){
        while (true) {
            std::this_thread::sleep_for(std::chrono::minutes(10));
            std::lock_guard<std::mutex> lock(cache_mutex);
            SQLite::Statement q1(db, "UPDATE relation SET unread=unread+? WHERE user_id=? AND friend_id=?");
            SQLite::Transaction tr(db);
            for (auto& item : cache) {
                q1.bind(1, item.second.second);
                q1.bind(2, item.first);
                q1.bind(3, item.second.first);
                try {
                    q1.executeStep();
                }
                catch (const std::exception&) {
                }
                q1.reset();
            }
            tr.commit();
            cache.clear();
        }
    }
};

#endif
