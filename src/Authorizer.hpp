#ifndef AUTHORIZER_HPP
#define AUTHORIZER_HPP

#include <iostream>
#include <random>

#include <sqlite3.h>
#include <SQLiteCpp/SQLiteCpp.h>

class Authorizer {
public:
    enum class Result {
        SUCCESS,

        USERNAME_DUPLICATE,
        USERNAME_INVALID,
        PASSWORD_EMPTY,

        USER_DONOT_EXIST,
        PASSWORD_INCORRECT,

        SESSDATA_INVALID
    };

private:
    SQLite::Database db{"users.db", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE};

public:
    Authorizer() { }

    Result new_user(const std::string& user_name, const std::string& password, const std::string& icon = "") {
        if (user_name.empty() || user_name.size() > 40)
            return Result::USERNAME_INVALID;
        if (password.empty())
            return Result::PASSWORD_EMPTY;

        SQLite::Statement query_dup(db, "SELECT * FROM user WHERE user_name=?");
        query_dup.bind(1, user_name);
        if (query_dup.executeStep())
            return Result::USERNAME_DUPLICATE;

        SQLite::Transaction tr(db);
        SQLite::Statement insert(db, "INSERT INTO user (user_name, password, icon) VALUES (?, ?, ?)");
        insert.bind(1, user_name);
        insert.bind(2, password);
        insert.bind(3, icon);
        insert.exec();
        tr.commit();

        return Result::SUCCESS;
    }

    Result log_in(const std::string& user_name, const std::string& password, int& id, unsigned int& sessdata) {
        SQLite::Statement query_exist(db, "SELECT id, password FROM user WHERE user_name=?");
        query_exist.bind(1, user_name);
        if (!query_exist.executeStep())
            return Result::USER_DONOT_EXIST;

        id = query_exist.getColumn(0);
        std::string password_correct = query_exist.getColumn(1);
        if (password_correct != password)
            return Result::PASSWORD_INCORRECT;

        time_t t;
        time(&t);
        std::mt19937 mt_rand(std::random_device{}());
        sessdata = (static_cast<unsigned long long>(mt_rand() + std::hash<std::string>()(user_name)) << 16) + t;
        SQLite::Transaction tr(db);
        SQLite::Statement insert(db, "UPDATE user SET sessdata=? WHERE id=?");
        insert.bind(1, sessdata);
        insert.bind(2, id);
        insert.exec();
        tr.commit();

        return Result::SUCCESS;
    }

    Result log_out(unsigned int sessdata) {
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

        return Result::SUCCESS;
    }

    Result authorize(const unsigned int& sessdata, int& id, std::string& user_name) {
        SQLite::Statement query_exist(db, "SELECT id, user_name FROM user WHERE sessdata=?");
        query_exist.bind(1, sessdata);
        if (!query_exist.executeStep())
            return Result::SESSDATA_INVALID;

        id = query_exist.getColumn(0);
        std::string name = query_exist.getColumn(1);
        user_name = name;

        return Result::SUCCESS;
    }

    Result set_icon(int id, const std::string& icon = "") {
        SQLite::Transaction tr(db);
        SQLite::Statement modify(db, "UPDATE user SET icon=? WHERE id=?");
        modify.bind(1, icon);
        modify.bind(2, id);
        modify.exec();
        tr.commit();

        return Result::SUCCESS;
    }

    Result get_icon(int id, std::string& icon) {
        SQLite::Statement query(db, "SELECT icon FROM user WHERE id=?");
        query.bind(1, id);
        query.executeStep();
        std::string ico = query.getColumn(0);
        icon = ico;
     
        return Result::SUCCESS;
    }

    Result get_icon(const std::string& user_name, std::string& icon) {
        SQLite::Statement query(db, "SELECT icon FROM user WHERE user_name=?");
        query.bind(1, user_name);
        query.executeStep();
        std::string ico = query.getColumn(0);
        icon = ico;

        return Result::SUCCESS;
    }

    Result set_user_name(int id, const std::string& new_user_name) {
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

        return Result::SUCCESS;
    }
};

#endif
