#include "database.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>

Database::Database(std::string path) : path_(std::move(path)) {
    if (sqlite3_open(path_.c_str(), &database_) != SQLITE_OK)
        throw std::runtime_error("Unable to open SQLite database");
    sqlite3_busy_timeout(database_, 5000);
}

Database::~Database() {
    if (database_) sqlite3_close(database_);
}

void Database::initialize() {
    std::ifstream file("database/schema.sql");
    if (!file) throw std::runtime_error("Unable to open database/schema.sql");
    std::string sql((std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());

    char* error = nullptr;
    if (sqlite3_exec(database_, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
        std::string message = error ? error : "Database initialization failed";
        sqlite3_free(error);
        throw std::runtime_error(message);
    }
}
