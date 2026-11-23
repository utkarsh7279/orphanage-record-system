#include "services.h"
#include "password.h"

#include <openssl/rand.h>

#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {

bool isIsoDate(const std::string& value) {
    if (value.size() != 10) return false;
    if (value[4] != '-' || value[7] != '-') return false;
    for (size_t i = 0; i < value.size(); ++i) {
        if (i == 4 || i == 7) continue;
        if (value[i] < '0' || value[i] > '9') return false;
    }
    return true;
}

bool isRealIsoDate(const std::string& value) {
    if (!isIsoDate(value)) return false;
    int year = std::stoi(value.substr(0, 4));
    int month = std::stoi(value.substr(5, 2));
    int day = std::stoi(value.substr(8, 2));
    if (month < 1 || month > 12) return false;
    static const int days[] = {31, 28, 31, 30, 31, 30,
                               31, 31, 30, 31, 30, 31};
    int maximum = days[month - 1];
    if (month == 2 &&
        (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0)))
        maximum = 29;
    return day >= 1 && day <= maximum;
}

std::string text(sqlite3_stmt* statement, int column) {
    const unsigned char* value = sqlite3_column_text(statement, column);
    return value ? reinterpret_cast<const char*>(value) : "";
}

int integer(sqlite3_stmt* statement, int column) {
    return sqlite3_column_int(statement, column);
}

std::string randomToken(size_t bytes = 32) {
    unsigned char buffer[32];
    if (bytes > sizeof(buffer) ||
        RAND_bytes(buffer, static_cast<int>(bytes)) != 1)
        throw std::runtime_error("Unable to create session");
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (size_t i = 0; i < bytes; ++i)
        out << std::setw(2) << static_cast<unsigned int>(buffer[i]);
    return out.str();
}

void bindText(sqlite3_stmt* statement, int index, const std::string& value) {
    if (sqlite3_bind_text(statement, index, value.c_str(),
                          static_cast<int>(value.size()),
                          SQLITE_TRANSIENT) != SQLITE_OK)
        throw std::runtime_error("Unable to prepare database value");
}

void bindInt(sqlite3_stmt* statement, int index, int value) {
    if (sqlite3_bind_int(statement, index, value) != SQLITE_OK)
        throw std::runtime_error("Unable to prepare database value");
}

void logActivity(Database& database, int userId, const std::string& action,
                 const std::string& details) {
    sqlite3_stmt* statement = nullptr;
    const char* sql =
        "INSERT INTO activity_log(user_id, action, details) VALUES(?1, ?2, ?3)";
    if (sqlite3_prepare_v2(database.handle(), sql, -1,
                           &statement, nullptr) != SQLITE_OK)
        throw std::runtime_error("Unable to prepare activity log");
    bindInt(statement, 1, userId);
    bindText(statement, 2, action);
    bindText(statement, 3, details);
    if (sqlite3_step(statement) != SQLITE_DONE) {
        sqlite3_finalize(statement);
        throw std::runtime_error("Unable to write activity log");
    }
    sqlite3_finalize(statement);
}

}  // namespace

AuthService::AuthService(Database& database) : database_(database) {}

void AuthService::seedDemoData() {
    const char* findSql =
        "SELECT id FROM users WHERE username = ?1";
    sqlite3_stmt* statement = nullptr;

    struct SeedUser {
        const char* name;
        const char* username;
        const char* password;
        const char* role;
    };
    SeedUser users[] = {
        {"Demo Administrator", "admin", "demo1234", "Administrator"},
        {"Demo Staff", "staff", "demo1234", "Staff"}
    };

    for (const auto& seed : users) {
        if (sqlite3_prepare_v2(database_.handle(), findSql, -1,
                               &statement, nullptr) != SQLITE_OK)
            throw std::runtime_error("Unable to query seed users");
        bindText(statement, 1, seed.username);
        int result = sqlite3_step(statement);
        sqlite3_finalize(statement);
        statement = nullptr;
        if (result == SQLITE_ROW) continue;

        const char* insertSql =
            "INSERT INTO users(full_name, username, password_hash, role, active)"
            " VALUES(?1, ?2, ?3, ?4, 1)";
        if (sqlite3_prepare_v2(database_.handle(), insertSql, -1,
                               &statement, nullptr) != SQLITE_OK)
            throw std::runtime_error("Unable to prepare seed user");
        bindText(statement, 1, seed.name);
        bindText(statement, 2, seed.username);
        std::string passwordHash = PasswordHash::hash(seed.password);
        bindText(statement, 3, passwordHash);
        bindText(statement, 4, seed.role);
        if (sqlite3_step(statement) != SQLITE_DONE)
            throw std::runtime_error("Unable to insert seed user");
        sqlite3_finalize(statement);
        statement = nullptr;
    }
}

std::string AuthService::login(const std::string& username,
                               const std::string& password) {
    if (username.empty() || password.empty())
        throw ValidationException("Username and password are required");

    const char* sql =
        "SELECT id, full_name, username, password_hash, role, active"
        " FROM users WHERE username = ?1";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database_.handle(), sql, -1,
                           &statement, nullptr) != SQLITE_OK)
        throw std::runtime_error("Unable to query login");
    bindText(statement, 1, username);

    if (sqlite3_step(statement) != SQLITE_ROW) {
        sqlite3_finalize(statement);
        throw ValidationException("Invalid username or password");
    }
    if (integer(statement, 5) != 1) {
        sqlite3_finalize(statement);
        throw ValidationException("This account is inactive");
    }

    int id = integer(statement, 0);
    std::string name = text(statement, 1);
    std::string normalizedUsername = text(statement, 2);
    std::string passwordHash = text(statement, 3);
    std::string role = text(statement, 4);
    sqlite3_finalize(statement);

    if (!PasswordHash::verify(password, passwordHash))
        throw ValidationException("Invalid username or password");

    std::string token = randomToken();
    sessions_[token] = Session{id, name, normalizedUsername, role};
    return token;
}

void AuthService::logout(const std::string& token) {
    sessions_.erase(token);
}

Session AuthService::session(const std::string& token) const {
    auto found = sessions_.find(token);
    if (found == sessions_.end())
        throw ValidationException("Authentication required");
    return found->second;
}

bool AuthService::hasPermission(const std::string& token,
                                const std::string& role) const {
    auto found = sessions_.find(token);
    if (found == sessions_.end()) return false;
    if (role.empty()) return true;
    return found->second.role == role;
}

json AuthService::listUsers(const std::string& token) {
    if (!hasPermission(token, "Administrator"))
        throw ValidationException("Administrator permission required");

    sqlite3_stmt* statement = nullptr;
    const char* sql =
        "SELECT id, full_name, username, role, active, created_at"
        " FROM users ORDER BY full_name";
    if (sqlite3_prepare_v2(database_.handle(), sql, -1,
                           &statement, nullptr) != SQLITE_OK)
        throw std::runtime_error("Unable to list users");

    json users = json::array();
    while (sqlite3_step(statement) == SQLITE_ROW) {
        users.push_back({
            {"id", integer(statement, 0)},
            {"name", text(statement, 1)},
            {"username", text(statement, 2)},
            {"role", text(statement, 3)},
            {"active", integer(statement, 4) == 1},
            {"createdAt", text(statement, 5)}
        });
    }
    sqlite3_finalize(statement);
    return users;
}

json AuthService::createUser(const std::string& token,
                             const std::string& name,
                             const std::string& username,
                             const std::string& password,
                             const std::string& role) {
    if (!hasPermission(token, "Administrator"))
        throw ValidationException("Administrator permission required");
    if (name.empty() || username.empty() || password.empty())
        throw ValidationException("Name, username, and password are required");
    if (username.size() > 80 || password.size() < 8)
        throw ValidationException("Username is too long or password is fewer than 8 characters");
    if (role != "Administrator" && role != "Staff")
        throw ValidationException("Role must be Administrator or Staff");

    sqlite3_stmt* statement = nullptr;
    const char* sql =
        "INSERT INTO users(full_name, username, password_hash, role, active)"
        " VALUES(?1, ?2, ?3, ?4, 1)";
    if (sqlite3_prepare_v2(database_.handle(), sql, -1,
                           &statement, nullptr) != SQLITE_OK)
        throw std::runtime_error("Unable to create user");
    bindText(statement, 1, name);
    bindText(statement, 2, username);
    bindText(statement, 3, PasswordHash::hash(password));
    bindText(statement, 4, role);
    int result = sqlite3_step(statement);
    if (result == SQLITE_CONSTRAINT) {
        sqlite3_finalize(statement);
        throw ValidationException("That username is already in use");
    }
    if (result != SQLITE_DONE) {
        sqlite3_finalize(statement);
        throw std::runtime_error("Unable to create user");
    }
    int id = static_cast<int>(sqlite3_last_insert_rowid(database_.handle()));
    sqlite3_finalize(statement);
    return {{"id", id}, {"name", name}, {"username", username}, {"role", role}};
}

RecordService::RecordService(Database& database) : database_(database) {}

json RecordService::list(const std::string& status) {
    return search("", status, "");
}

json RecordService::search(const std::string& query) {
    return search(query, "Active", "");
}

json RecordService::search(const std::string& query,
                           const std::string& status,
                           const std::string& gender) {
    if (status != "" && status != "Active" && status != "Archived")
        throw ValidationException("Status must be Active or Archived");
    if (gender != "" && gender != "Female" && gender != "Male" &&
        gender != "Other" && gender != "Not specified")
        throw ValidationException("Invalid gender filter");

    std::string sql =
        "SELECT id, first_name, last_name, date_of_birth, gender, status,"
        " notes, created_at, updated_at FROM records WHERE 1=1";
    if (!query.empty())
        sql += " AND (first_name LIKE ?1 OR last_name LIKE ?1"
               " OR CAST(id AS TEXT) LIKE ?1)";
    if (!status.empty()) sql += " AND status = ?2";
    if (!gender.empty()) sql += " AND gender = ?3";
    sql += " ORDER BY updated_at DESC, id DESC";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database_.handle(), sql.c_str(), -1,
                           &statement, nullptr) != SQLITE_OK)
        throw std::runtime_error("Unable to search records");
    if (!query.empty()) bindText(statement, 1, "%" + query + "%");
    if (!status.empty()) bindText(statement, 2, status);
    if (!gender.empty()) bindText(statement, 3, gender);

    json records = json::array();
    while (sqlite3_step(statement) == SQLITE_ROW) {
        records.push_back({
            {"id", integer(statement, 0)},
            {"firstName", text(statement, 1)},
            {"lastName", text(statement, 2)},
            {"dateOfBirth", text(statement, 3)},
            {"gender", text(statement, 4)},
            {"status", text(statement, 5)},
            {"notes", text(statement, 6)},
            {"createdAt", text(statement, 7)},
            {"updatedAt", text(statement, 8)}
        });
    }
    sqlite3_finalize(statement);
    return records;
}

json RecordService::get(int id) {
    sqlite3_stmt* statement = nullptr;
    const char* sql =
        "SELECT id, first_name, last_name, date_of_birth, gender, status,"
        " notes, created_at, updated_at FROM records WHERE id = ?1";
    if (sqlite3_prepare_v2(database_.handle(), sql, -1,
                           &statement, nullptr) != SQLITE_OK)
        throw std::runtime_error("Unable to load record");
    bindInt(statement, 1, id);
    if (sqlite3_step(statement) != SQLITE_ROW) {
        sqlite3_finalize(statement);
        throw ValidationException("Record not found");
    }
    json record = {
        {"id", integer(statement, 0)},
        {"firstName", text(statement, 1)},
        {"lastName", text(statement, 2)},
        {"dateOfBirth", text(statement, 3)},
        {"gender", text(statement, 4)},
        {"status", text(statement, 5)},
        {"notes", text(statement, 6)},
        {"createdAt", text(statement, 7)},
        {"updatedAt", text(statement, 8)}
    };
    sqlite3_finalize(statement);
    return record;
}

json RecordService::create(const Record& record, int staffId) {
    const char* sql =
        "INSERT INTO records(first_name, last_name, date_of_birth, gender,"
        " status, notes, created_by, updated_by)"
        " VALUES(?1, ?2, ?3, ?4, 'Active', ?5, ?6, ?6)";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database_.handle(), sql, -1,
                           &statement, nullptr) != SQLITE_OK)
        throw std::runtime_error("Unable to create record");
    bindText(statement, 1, record.firstName());
    bindText(statement, 2, record.lastName());
    bindText(statement, 3, record.dateOfBirth());
    bindText(statement, 4, record.gender());
    bindText(statement, 5, record.notes());
    bindInt(statement, 6, staffId);
    if (sqlite3_step(statement) != SQLITE_DONE) {
        sqlite3_finalize(statement);
        throw std::runtime_error("Unable to create record");
    }
    int id = static_cast<int>(sqlite3_last_insert_rowid(database_.handle()));
    sqlite3_finalize(statement);
    logActivity(database_, staffId, "Record created",
                "Created record " + std::to_string(id));
    return get(id);
}

json RecordService::update(int id, const Record& record, int staffId) {
    get(id);
    const char* sql =
        "UPDATE records SET first_name=?1, last_name=?2, date_of_birth=?3,"
        " gender=?4, notes=?5, updated_by=?6,"
        " updated_at=CURRENT_TIMESTAMP WHERE id=?7";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database_.handle(), sql, -1,
                           &statement, nullptr) != SQLITE_OK)
        throw std::runtime_error("Unable to update record");
    bindText(statement, 1, record.firstName());
    bindText(statement, 2, record.lastName());
    bindText(statement, 3, record.dateOfBirth());
    bindText(statement, 4, record.gender());
    bindText(statement, 5, record.notes());
    bindInt(statement, 6, staffId);
    bindInt(statement, 7, id);
    if (sqlite3_step(statement) != SQLITE_DONE) {
        sqlite3_finalize(statement);
        throw std::runtime_error("Unable to update record");
    }
    sqlite3_finalize(statement);
    logActivity(database_, staffId, "Record updated",
                "Updated record " + std::to_string(id));
    return get(id);
}

json RecordService::archive(int id, int staffId) {
    get(id);
    sqlite3_stmt* statement = nullptr;
    const char* sql =
        "UPDATE records SET status='Archived', updated_by=?1,"
        " updated_at=CURRENT_TIMESTAMP WHERE id=?2";
    if (sqlite3_prepare_v2(database_.handle(), sql, -1,
                           &statement, nullptr) != SQLITE_OK)
        throw std::runtime_error("Unable to archive record");
    bindInt(statement, 1, staffId);
    bindInt(statement, 2, id);
    if (sqlite3_step(statement) != SQLITE_DONE) {
        sqlite3_finalize(statement);
        throw std::runtime_error("Unable to archive record");
    }
    sqlite3_finalize(statement);
    logActivity(database_, staffId, "Record archived",
                "Archived record " + std::to_string(id));
    return get(id);
}

json RecordService::remove(int id, int staffId) {
    json record = get(id);
    sqlite3_stmt* statement = nullptr;
    const char* sql = "DELETE FROM records WHERE id=?1";
    if (sqlite3_prepare_v2(database_.handle(), sql, -1,
                           &statement, nullptr) != SQLITE_OK)
        throw std::runtime_error("Unable to delete record");
    bindInt(statement, 1, id);
    if (sqlite3_step(statement) != SQLITE_DONE) {
        sqlite3_finalize(statement);
        throw std::runtime_error("Unable to delete record");
    }
    sqlite3_finalize(statement);
    logActivity(database_, staffId, "Record deleted",
                "Deleted record " + std::to_string(id));
    return record;
}

Report::Report(Database& database) : database_(database) {}

json Report::generate() {
    return generate("", "");
}

json Report::generate(const std::string& from, const std::string& to) {
    if (!from.empty() && !isRealIsoDate(from))
        throw ValidationException("Start date must use YYYY-MM-DD format");
    if (!to.empty() && !isRealIsoDate(to))
        throw ValidationException("End date must use YYYY-MM-DD format");
    if (!from.empty() && !to.empty() && from > to)
        throw ValidationException("Start date must be before end date");

    json report;
    report["from"] = from;
    report["to"] = to;
    report["metrics"] = json::object();

    const char* countSql =
        "SELECT COUNT(*) FROM records"
        " WHERE (?1='' OR created_at >= ?1)"
        " AND (?2='' OR created_at < date(?2, '+1 day'))";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database_.handle(), countSql, -1,
                           &statement, nullptr) != SQLITE_OK)
        throw std::runtime_error("Unable to calculate report");
    bindText(statement, 1, from);
    bindText(statement, 2, to);
    if (sqlite3_step(statement) == SQLITE_ROW)
        report["metrics"]["totalCreated"] = integer(statement, 0);
    sqlite3_finalize(statement);

    const char* activeSql =
        "SELECT COUNT(*) FROM records WHERE status='Active'"
        " AND (?1='' OR created_at >= ?1)"
        " AND (?2='' OR created_at < date(?2, '+1 day'))";
    sqlite3_prepare_v2(database_.handle(), activeSql, -1,
                       &statement, nullptr);
    bindText(statement, 1, from);
    bindText(statement, 2, to);
    if (sqlite3_step(statement) == SQLITE_ROW)
        report["metrics"]["active"] = integer(statement, 0);
    sqlite3_finalize(statement);

    const char* archiveSql =
        "SELECT COUNT(*) FROM records WHERE status='Archived'"
        " AND (?1='' OR created_at >= ?1)"
        " AND (?2='' OR created_at < date(?2, '+1 day'))";
    sqlite3_prepare_v2(database_.handle(), archiveSql, -1,
                       &statement, nullptr);
    bindText(statement, 1, from);
    bindText(statement, 2, to);
    if (sqlite3_step(statement) == SQLITE_ROW)
        report["metrics"]["archived"] = integer(statement, 0);
    sqlite3_finalize(statement);

    return report;
}

Manager::Manager(Database& database, AuthService& auth,
                 RecordService& records, Report& report)
    : database_(database), auth_(auth), records_(records), report_(report) {}

json Manager::dashboard() {
    json result;
    result["activeRecords"] =
        records_.search("", "Active", "").size();
    result["archivedRecords"] =
        records_.search("", "Archived", "").size();
    result["recentActivity"] = json::array();

    sqlite3_stmt* statement = nullptr;
    const char* sql =
        "SELECT id, first_name, last_name, updated_at"
        " FROM records ORDER BY updated_at DESC LIMIT 5";
    if (sqlite3_prepare_v2(database_.handle(), sql, -1,
                           &statement, nullptr) == SQLITE_OK) {
        while (sqlite3_step(statement) == SQLITE_ROW) {
            result["recentActivity"].push_back({
                {"id", integer(statement, 0)},
                {"name", text(statement, 1) + " " + text(statement, 2)},
                {"updatedAt", text(statement, 3)}
            });
        }
        sqlite3_finalize(statement);
    }
    return result;
}
