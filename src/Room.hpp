#ifndef ROOM_HPP
#define ROOM_HPP

#include <string>
#include <vector>
#include <memory>

#include <websocketpp/server.hpp>

using websocketpp::connection_hdl;

struct User {
    std::string     name;
    unsigned        room_id;
    connection_hdl  conn_hdl;
    User(const std::string& name, unsigned room_id, connection_hdl conn_hdl) :
        name(name), room_id(room_id), conn_hdl(conn_hdl) {
    }
};

struct Room {
    std::vector<User*> users;
    bool        have_password = false;
    std::string password;

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
};

#endif
