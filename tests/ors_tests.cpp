#include "database.h"
#include "domain.h"
#include "password.h"

#include <cstdio>
#include <iostream>
#include <stdexcept>

namespace {

int failures = 0;

void require(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << "\n";
    }
}

}  // namespace

int main() {
    try {
        std::string password = "student-demo-password";
        std::string hash = PasswordHash::hash(password);
        require(hash.rfind("pbkdf2-sha256$", 0) == 0,
                "password hash has a PBKDF2 prefix");
        require(PasswordHash::verify(password, hash),
                "password verifies against its hash");
        require(!PasswordHash::verify("wrong-password", hash),
                "wrong password is rejected");

        Record valid("Asha", "Kapoor", "2018-04-12",
                     "Female", "Fictional test record");
        require(valid.firstName() == "Asha",
                "Record constructor initializes encapsulated state");

        bool invalidNameRejected = false;
        try {
            Record("", "Kapoor", "", "Not specified", "");
        } catch (const ValidationException&) {
            invalidNameRejected = true;
        }
        require(invalidNameRejected,
                "record validation rejects an empty name");

        bool invalidDateRejected = false;
        try {
            Record("Asha", "Kapoor", "2026-02-30",
                   "Female", "");
        } catch (const ValidationException&) {
            invalidDateRejected = true;
        }
        require(invalidDateRejected,
                "record validation rejects an impossible date");

        const char* testDatabase = "/tmp/orphanage-ors-tests.db";
        std::remove(testDatabase);
        Database database(testDatabase);
        database.initialize();

        sqlite3_stmt* statement = nullptr;
        require(sqlite3_prepare_v2(database.handle(),
                                   "SELECT COUNT(*) FROM sqlite_master "
                                   "WHERE type='table' AND name='records'",
                                   -1, &statement, nullptr) == SQLITE_OK,
                "SQLite records table can be queried");
        if (statement) {
            require(sqlite3_step(statement) == SQLITE_ROW &&
                        sqlite3_column_int(statement, 0) == 1,
                    "schema initialization creates the records table");
            sqlite3_finalize(statement);
        }
        std::remove(testDatabase);
    } catch (const std::exception& error) {
        ++failures;
        std::cerr << "FAIL: unexpected exception: " << error.what() << "\n";
    }

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All 5 automated checks passed\n";
    return 0;
}
