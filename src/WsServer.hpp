#ifndef WS_SERVER_HPP
#define WS_SERVER_HPP

#include <string>
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <thread>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <json/json.h>

#include "RoomManager.hpp"
#include "Authorizer.hpp"
#include "ChatHistory.hpp"

typedef websocketpp::server<websocketpp::config::asio> server;
//typedef websocketpp::log::basic<websocketpp::concurrency::basic, websocketpp::log::elevel> basic_elog;

using std::placeholders::_1;
using std::placeholders::_2;
using websocketpp::connection_hdl;

struct Connection {
    int user_id;
    std::string user_name;
    unsigned conn_id;
    //connection_hdl hdl;
};

enum ActionType {
    SUBSCRIBE,
    UNSUBSCRIBE,
    MESSAGE
};

struct Action {
    Action(ActionType t, connection_hdl h) : type(t), hdl(h) {}
    Action(ActionType t, connection_hdl h, server::message_ptr m)
        : type(t), hdl(h), msg(m) {
    }

    ActionType             type;
    connection_hdl          hdl;
    server::message_ptr     msg;
};

struct Response {
    unsigned    conn_id;
    std::string payload;
};

class WsServer {
    std::map<connection_hdl, Connection, std::owner_less<connection_hdl>> connections;
    std::unordered_map<unsigned, connection_hdl> conn_id_2_hdl;
    std::unordered_multimap<int, unsigned> user_id_2_conn_id;

    server      svr;

    std::queue<Action>      actions;
    std::mutex              action_lock;
    std::condition_variable action_cond;

    std::mutex responses_lock;
    std::queue<Response>    responses;

    RoomManager<std::function<void(unsigned, const std::string&)>>
        room_manager;
    Authorizer& auth = Authorizer::get_instance();
    ChatHistory& chat_history = ChatHistory::get_instance();

    //basic_elog elogger;

public:
    WsServer():room_manager(std::bind(&WsServer::push_message, this, ::_1, ::_2)) {
        // Initialize Asio transport
        svr.init_asio();

        // Set callbacks
        svr.set_open_handler(std::bind(&WsServer::on_open, this, ::_1));
        svr.set_close_handler(std::bind(&WsServer::on_close, this, ::_1));
        svr.set_message_handler(std::bind(&WsServer::on_message, this, ::_1, ::_2));

        //elogger.set_channels(websocketpp::log::elevel::info);
    }

    void run(uint16_t port) {
        // listen on specified port
        svr.listen(port);

        // Start the server accept loop
        svr.start_accept();

        // Start the ASIO io_service run loop
        try {
            std::thread t1(std::bind(&WsServer::send_message, this));
            t1.detach();
            std::thread t2(std::bind(&decltype(room_manager)::check_empty_rooms, &room_manager));
            t2.detach();
            svr.run();
        }
        catch (const std::exception& e) {
            std::cout << e.what() << std::endl;
        }
    }

    void on_open(connection_hdl hdl) {
        {
            std::lock_guard<std::mutex> guard(action_lock);
            actions.push(Action(SUBSCRIBE, hdl));
        }
        action_cond.notify_one();
    }

    void on_close(connection_hdl hdl) {
        {
            std::lock_guard<std::mutex> guard(action_lock);
            actions.push(Action(UNSUBSCRIBE, hdl));
        }
        action_cond.notify_one();
    }

    void on_message(connection_hdl hdl, server::message_ptr msg) {
        {
            std::lock_guard<std::mutex> guard(action_lock);
            actions.push(Action(MESSAGE, hdl, msg));
        }
        action_cond.notify_one();
    }

    void process_message() {
        static unsigned conn_id = 0;

        while (true) {
            std::unique_lock<std::mutex> lock(action_lock);

            while (actions.empty()) 
                action_cond.wait(lock);

            Action a = actions.front();
            actions.pop();
            lock.unlock();

            // Process
            switch (a.type) {

            case MESSAGE: {
                auto& payload = a.msg->get_payload();
                Json::Reader reader;
                Json::Value msg;

                if (!reader.parse(payload, msg) || !msg.isMember("message_type")) {
                    //elogger.write(websocketpp::log::elevel::info, "Invalid message format. ");
                    break;
                }

                std::string message_type = msg["message_type"].asString();

                // Authorized connection
                if (connections.count(a.hdl)) {
                    // 处理私信聊天
                    if (message_type == "WHISPER_MESSAGE") {
                        int receiver_id = msg["receiver_id"].asInt();
                        msg["message"]["user_name"] = connections[a.hdl].user_name;
                        msg["message"]["user_id"] = connections[a.hdl].user_id;
                        msg["message"]["timestamp"] = static_cast<int64_t>(chat_history.get_timestamp());
                        chat_history.new_chat_message(
                            connections[a.hdl].user_id,
                            receiver_id,
                            Json::FastWriter().write(msg["message"])
                        );
                        auth.add_one_unread(receiver_id, connections[a.hdl].user_id);
                        try {
                            svr.send(
                                a.hdl, Json::FastWriter().write(msg),
                                websocketpp::frame::opcode::TEXT
                            );
                        }
                        catch (const std::exception& e) {
                        };
                        if (user_id_2_conn_id.count(receiver_id)) {
                            auto [begin, end] = user_id_2_conn_id.equal_range(receiver_id);
                            for (auto it = begin; it != end; ++it)
                                push_message(
                                    it->second,
                                    Json::FastWriter().write(msg)
                                );
                        }
                    }
                    else if (message_type == "READ_WHISPER_MESSAGES") {
                        int friend_id = msg["friend_id"].asInt();
                        auth.clear_unread(connections[a.hdl].user_id, friend_id);
                    }

                    // 交给RoomManager类处理信息
                    else
                        room_manager.process_message(
                            connections[a.hdl].conn_id,
                            connections[a.hdl].user_name,
                            connections[a.hdl].user_id,
                            message_type,
                            msg
                        );
                }
                // Need Authorize
                else {
                    if (message_type != "AUTHORIZE") {
                        // 请先登录
                        Json::Value res;
                        res["message_type"] = "PLEASE_LOG_IN";
                        try {
                            svr.send(
                                a.hdl, Json::FastWriter().write(res),
                                websocketpp::frame::opcode::TEXT
                            );
                        }
                        catch (const std::exception& e) {
                        }
                        break;
                    }
                    
                    // 交给Auth类处理
                    unsigned sessdata = msg["sessdata"].asUInt();
                    int id;
                    std::string user_name;
                    Authorizer::Result result =
                        auth.authorize(sessdata, id, user_name);

                    Json::Value res;
                    res["message_type"] = "AUTHORIZE_RES";
                    if (result == Authorizer::Result::SUCCESS) {
                        res["success"] = true;
                        res["id"] = id;
                        res["user_name"] = user_name;
                        connections[a.hdl] = { id, user_name, ++conn_id };
                        conn_id_2_hdl[conn_id] = a.hdl;
                        user_id_2_conn_id.emplace(id, conn_id);
                    }
                    else {
                        res["success"] = false;
                    }
                    try {
                        svr.send(
                            a.hdl, Json::FastWriter().write(res),
                            websocketpp::frame::opcode::TEXT
                        );
                    }
                    catch (const std::exception& e) {
                    }
                }
                break;
            }

            case UNSUBSCRIBE: {
                if (connections.count(a.hdl)) {
                    room_manager.process_close(connections[a.hdl].conn_id, connections[a.hdl].user_id);
                    conn_id_2_hdl.erase(connections[a.hdl].conn_id);
                    user_id_2_conn_id.erase(connections[a.hdl].conn_id);
                    connections.erase(a.hdl);
                }
                break;
            }

            default:
                break;
            }
        }
    }

    void send_message() {
        // Send responses
        while (true) {
            {
                std::lock_guard<std::mutex> lock(responses_lock);
                while (!responses.empty()) {
                    auto& res = responses.front();
                    try {
                        svr.send(
                            conn_id_2_hdl[res.conn_id],
                            res.payload,
                            websocketpp::frame::opcode::TEXT
                        );
                    }
                    catch (const std::exception& e) {
                    }
                    responses.pop();
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    void push_message(unsigned conn_id, const std::string& payload) {
        std::lock_guard<std::mutex> lock(responses_lock);
        responses.push({ conn_id, payload });
    }
};

#endif
