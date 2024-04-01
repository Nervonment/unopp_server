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

    server      svr;

    std::queue<Action>      actions;
    std::mutex              action_lock;
    std::condition_variable action_cond;

    std::queue<Response>    responses;

    RoomManager<std::function<void(unsigned, const std::string&)>>
        room_manager;
    Authorizer& auth = Authorizer::get_instance();

    //basic_elog elogger;

public:
    WsServer():room_manager(std::bind(&WsServer::send_message, this, ::_1, ::_2)) {
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
                auto payload = a.msg->get_payload();
                Json::Reader reader;
                Json::Value msg;

                if (!reader.parse(payload, msg) || !msg.isMember("message_type")) {
                    //elogger.write(websocketpp::log::elevel::info, "Invalid message format. ");
                    break;
                }

                std::string message_type = msg["message_type"].asString();

                // Authorized connection
                if (connections.count(a.hdl)) {
                    // 交给RoomManager类处理信息
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
                        svr.send(
                            a.hdl, Json::FastWriter().write(res),
                            websocketpp::frame::opcode::TEXT
                        );
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
                    }
                    else {
                        res["success"] = false;
                    }
                    svr.send(
                        a.hdl, Json::FastWriter().write(res),
                        websocketpp::frame::opcode::TEXT
                    );
                }
                break;
            }

            case UNSUBSCRIBE: {
                if (connections.count(a.hdl)) {
                    room_manager.process_close(connections[a.hdl].conn_id, connections[a.hdl].user_id);
                    conn_id_2_hdl.erase(connections[a.hdl].conn_id);
                    connections.erase(a.hdl);
                }
                break;
            }

            default:
                break;
            }

            // Send responses
            while (!responses.empty()) {
                auto& res = responses.front();
                try {
                    svr.send(
                        conn_id_2_hdl[res.conn_id],
                        res.payload,
                        websocketpp::frame::opcode::TEXT
                    );
                }
                catch (std::exception& e) {
                }
                responses.pop();
            }
        }
    }

    void send_message(unsigned conn_id, const std::string& payload) {
        responses.push({ conn_id, payload });
    }
};

#endif
