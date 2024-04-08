#ifndef CHAT_HISTORY_HPP
#define CHAT_HISTORY_HPP

#include <mutex>
#include <string>
#include <map>
#include <unordered_map>
#include <memory>
#include <thread>
#include <functional>

#include <sqlite3.h>
#include <SQLiteCpp/SQLiteCpp.h>
#include <json/json.h>

struct ChatMessage {
    int sender_id;
    int receiver_id;
    std::string message;
    long long timestamp;
};

class ChatHistory {
    std::mutex db_mutex;
    SQLite::Database db{ "chat.db", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE };

    std::mutex cache_mutex;
    std::vector<ChatMessage> cache;
    std::unordered_multimap<int, size_t> index;

    ChatHistory() {
        std::thread t(std::bind(&ChatHistory::write_database, this));
        t.detach();
    }
public:
    static ChatHistory& get_instance() {
        static ChatHistory instance;
        return instance;
    }

    enum class Result {
        SUCCESS,
        FAILED,


    };

    long long get_timestamp() {
        return static_cast<long long>(
            std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())
            );
    }

    Result new_chat_message(int sender_id, int receiver_id, const std::string& message) {
        {
            std::lock_guard<std::mutex> lock(cache_mutex);
            cache.push_back({ sender_id, receiver_id, message, get_timestamp() });
            index.emplace(sender_id, cache.size() - 1);
            index.emplace(receiver_id, cache.size() - 1);
        }

        return Result::SUCCESS;
    }

    void write_database() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::minutes(10));

            std::lock_guard<std::mutex> lock(cache_mutex);

            SQLite::Statement q1(db, "INSERT INTO chat (sender_id, receiver_id, timestamp, message) VALUES (?,?,?,?)");
            std::lock_guard<std::mutex> lock1(db_mutex);
            SQLite::Transaction tr(db);
            for (size_t i = 0; i < cache.size(); ++i) {
                auto& item = cache[cache.size() - i - 1];
                q1.bind(1, item.sender_id);
                q1.bind(2, item.receiver_id);
                q1.bind(3, static_cast<int64_t>(item.timestamp));
                q1.bind(4, item.message);
                try {
                    q1.executeStep();
                }
                catch (const std::exception&) {
                }
                q1.reset();
            }
            tr.commit();
            cache.clear();
            index.clear();
        }
    }

    Json::Value get_chat_message(int user_id, long long latest_timestamp) {
        Json::Value res;
        Json::Reader().parse("{}", res);

        // 缓存中的记录会全部被返回
        {
            std::lock_guard<std::mutex> lock(cache_mutex);
            auto [begin, end] = index.equal_range(user_id);
            for (auto it = begin; it != end; ++it)
                if (cache[it->second].timestamp < latest_timestamp) {
                    Json::Value item;
                    Json::Reader().parse(cache[it->second].message, item);
                    item["timestamp"] = static_cast<int64_t>(cache[it->second].timestamp);
                    int friend_id = cache[it->second].sender_id == user_id ?
                        cache[it->second].receiver_id : cache[it->second].sender_id;
                    res[std::to_string(friend_id)].append(item);
                }
        }

        {
            std::lock_guard<std::mutex> lock(db_mutex);
            std::set<int> ids;
            SQLite::Statement q2(db, "SELECT DISTINCT receiver_id FROM chat WHERE sender_id=?");
            q2.bind(1, user_id);
            while (q2.executeStep())
                ids.insert(q2.getColumn(0));
            SQLite::Statement q3(db, "SELECT DISTINCT sender_id FROM chat WHERE receiver_id=?");
            q3.bind(1, user_id);
            while (q3.executeStep())
                ids.insert(q3.getColumn(0));

            for (auto& id : ids) {
                SQLite::Statement q1(db, "SELECT sender_id, receiver_id, timestamp, message FROM chat WHERE ((sender_id=? AND receiver_id=?) OR (receiver_id=? AND sender_id=?)) AND timestamp<? ORDER BY timestamp DESC");
                q1.bind(1, user_id);
                q1.bind(2, id);
                q1.bind(3, user_id);
                q1.bind(4, id);
                q1.bind(5, static_cast<int64_t>(latest_timestamp));
                int cnt = 0;
                while (q1.executeStep() && cnt < 20) {
                    int sender_id = q1.getColumn(0);
                    int receiver_id = q1.getColumn(1);
                    long long timestamp = q1.getColumn(2).getInt64();
                    std::string content = q1.getColumn(3);
                    int friend_id = sender_id == user_id ? receiver_id : sender_id;
                    Json::Value item;
                    Json::Reader().parse(content, item);
                    item["timestamp"] = static_cast<int64_t>(timestamp);
                    res[std::to_string(friend_id)].append(item);
                    ++cnt;
                }
            }
        }
        return res;
    }

    Json::Value get_20_chat_messages(int user_id, int friend_id, long long latest_timestamp) {
        Json::Value res;
        res.resize(0);
        {
            std::lock_guard<std::mutex> lock(db_mutex);

            SQLite::Statement q1(db, "SELECT sender_id, receiver_id, timestamp, message FROM chat WHERE ((sender_id=? AND receiver_id=?) OR (receiver_id=? AND sender_id=?)) AND timestamp<? ORDER BY timestamp DESC");
            q1.bind(1, user_id);
            q1.bind(2, friend_id);
            q1.bind(3, user_id);
            q1.bind(4, friend_id);
            q1.bind(5, static_cast<int64_t>(latest_timestamp));
            int cnt = 0;
            while (q1.executeStep() && cnt < 20) {
                int sender_id = q1.getColumn(0);
                int receiver_id = q1.getColumn(1);
                long long timestamp = q1.getColumn(2).getInt64();
                std::string content = q1.getColumn(3);
                Json::Value item;
                Json::Reader().parse(content, item);
                item["timestamp"] = static_cast<int64_t>(timestamp);
                res.append(item);
                ++cnt;
            }
        }
        return res;
    }
};

#endif
