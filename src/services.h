#pragma once

#include "database.h"
#include "domain.h"

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

using json = nlohmann::json;

struct Session {
    int userId;
    std::string name;
    std::string username;
    std::string role;
};

class AuthService {
public:
    explicit AuthService(Database& database);
    void seedDemoData();
    std::string login(const std::string& username, const std::string& password);
    void logout(const std::string& token);
    bool hasPermission(const std::string& token, const std::string& role = "") const;
    Session session(const std::string& token) const;
    json listUsers(const std::string& token);
    json createUser(const std::string& token, const std::string& name,
                    const std::string& username, const std::string& password,
                    const std::string& role);

private:
    Database& database_;
    mutable std::unordered_map<std::string, Session> sessions_;
};

class RecordService {
public:
    explicit RecordService(Database& database);
    json list(const std::string& status = "Active");
    json search(const std::string& query);
    json search(const std::string& query, const std::string& status,
                const std::string& gender);
    json get(int id);
    json create(const Record& record, int staffId);
    json update(int id, const Record& record, int staffId);
    json archive(int id, int staffId);
    json remove(int id, int staffId);

private:
    Database& database_;
};

class Report {
public:
    explicit Report(Database& database);
    json generate();                     // Overload: all available records
    json generate(const std::string& from, const std::string& to);

private:
    Database& database_;
};

class Manager {
public:
    Manager(Database& database, AuthService& auth, RecordService& records,
            Report& report);

    json dashboard();

private:
    Database& database_;
    AuthService& auth_;
    RecordService& records_;
    Report& report_;
};
