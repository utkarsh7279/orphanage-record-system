#pragma once

#include <sqlite3.h>
#include <string>

class Database {
public:
    explicit Database(std::string path);
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    sqlite3* handle() const { return database_; }
    void initialize();

private:
    sqlite3* database_ = nullptr;
    std::string path_;
};
