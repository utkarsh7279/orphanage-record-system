#include "database.h"
#include "domain.h"
#include "services.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

using json = nlohmann::json;

namespace {

int portFromEnvironment() {
    const char* value = std::getenv("ORPHANAGE_PORT");
    if (!value) return 8080;
    try {
        int port = std::stoi(value);
        if (port < 1 || port > 65535)
            throw std::invalid_argument("out of range");
        return port;
    } catch (...) {
        throw std::runtime_error("ORPHANAGE_PORT must be an integer from 1 to 65535");
    }
}

std::string databasePath() {
    const char* value = std::getenv("ORPHANAGE_DB");
    return value ? value : "orphanage.db";
}

std::string webDirectory() {
    const char* value = std::getenv("ORPHANAGE_WEB_DIR");
    return value ? value : "web";
}

std::string trim(std::string value) {
    const std::string whitespace = " \t\n\r\f\v";
    size_t first = value.find_first_not_of(whitespace);
    if (first == std::string::npos) return "";
    size_t last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}

void sendJson(httplib::Response& response, int status, const json& body) {
    response.status = status;
    response.set_header("Cache-Control", "no-store");
    response.set_content(body.dump(), "application/json");
}

std::string bearerToken(const httplib::Request& request) {
    auto found = request.headers.find("Authorization");
    if (found == request.headers.end()) return "";
    const std::string& value = found->second;
    const std::string prefix = "Bearer ";
    if (value.rfind(prefix, 0) != 0) return "";
    return value.substr(prefix.size());
}

json parseBody(const httplib::Request& request) {
    if (request.body.empty()) return json::object();
    json body = json::parse(request.body, nullptr, false);
    if (body.is_discarded() || !body.is_object())
        throw ValidationException("Request body must be a JSON object");
    return body;
}

std::string requiredString(const json& body, const std::string& key) {
    if (!body.contains(key) || !body[key].is_string())
        throw ValidationException(key + " is required");
    return trim(body[key].get<std::string>());
}

int positiveId(const std::string& value) {
    try {
        int id = std::stoi(value);
        if (id > 0) return id;
    } catch (...) {}
    throw ValidationException("Invalid record id");
}

void installRoutes(httplib::Server& server, Database& database,
                   AuthService& auth, RecordService& records,
                   Report& report, Manager& manager) {
    server.set_mount_point("/", webDirectory());

    server.Get("/api/health", [](const httplib::Request&,
                                 httplib::Response& response) {
        sendJson(response, 200,
                 json{{"status", "ok"},
                      {"application", "Orphanage Record System"}});
    });

    server.Post("/api/auth/login", [&auth](const httplib::Request& request,
                                           httplib::Response& response) {
        try {
            json body = parseBody(request);
            std::string username = requiredString(body, "username");
            std::string password = requiredString(body, "password");
            std::string token = auth.login(username, password);
            Session session = auth.session(token);
            sendJson(response, 200,
                     json{{"token", token},
                          {"user", {{"name", session.name},
                                    {"username", session.username},
                                    {"role", session.role}}}});
        } catch (const ValidationException& error) {
            sendJson(response, 401, json{{"error", error.what()}});
        } catch (const std::exception&) {
            sendJson(response, 500,
                     json{{"error", "Login could not be completed"}});
        }
    });

    server.Post("/api/auth/logout", [&auth](const httplib::Request& request,
                                            httplib::Response& response) {
        auth.logout(bearerToken(request));
        sendJson(response, 200, json{{"status", "logged_out"}});
    });

    server.Get("/api/auth/me", [&auth](const httplib::Request& request,
                                       httplib::Response& response) {
        try {
            Session session = auth.session(bearerToken(request));
            sendJson(response, 200,
                     json{{"name", session.name},
                          {"username", session.username},
                          {"role", session.role}});
        } catch (const ValidationException& error) {
            sendJson(response, 401, json{{"error", error.what()}});
        }
    });

    server.Get("/api/dashboard", [&manager, &auth](const httplib::Request& request,
                                                   httplib::Response& response) {
        try {
            if (!auth.hasPermission(bearerToken(request)))
                throw ValidationException("Authentication required");
            sendJson(response, 200, manager.dashboard());
        } catch (const ValidationException& error) {
            sendJson(response, 401, json{{"error", error.what()}});
        } catch (const std::exception&) {
            sendJson(response, 500,
                     json{{"error", "Dashboard could not be loaded"}});
        }
    });

    server.Get("/api/records", [&records, &auth](const httplib::Request& request,
                                                 httplib::Response& response) {
        try {
            if (!auth.hasPermission(bearerToken(request)))
                throw ValidationException("Authentication required");
            std::string query = request.has_param("q") ? request.get_param_value("q") : "";
            std::string status = request.has_param("status")
                                     ? request.get_param_value("status") : "";
            std::string gender = request.has_param("gender")
                                     ? request.get_param_value("gender") : "";
            sendJson(response, 200,
                     json{{"items", records.search(query, status, gender)}});
        } catch (const ValidationException& error) {
            sendJson(response, 400, json{{"error", error.what()}});
        } catch (const std::exception&) {
            sendJson(response, 500,
                     json{{"error", "Records could not be loaded"}});
        }
    });

    server.Get("/api/records/:id", [&records, &auth](const httplib::Request& request,
                                                     httplib::Response& response) {
        try {
            if (!auth.hasPermission(bearerToken(request)))
                throw ValidationException("Authentication required");
            sendJson(response, 200,
                     records.get(positiveId(request.path_params.at("id"))));
        } catch (const ValidationException& error) {
            sendJson(response, 404, json{{"error", error.what()}});
        } catch (const std::exception&) {
            sendJson(response, 500,
                     json{{"error", "Record could not be loaded"}});
        }
    });

    server.Post("/api/records", [&records, &auth](const httplib::Request& request,
                                                  httplib::Response& response) {
        try {
            std::string token = bearerToken(request);
            if (!auth.hasPermission(token))
                throw ValidationException("Authentication required");
            json body = parseBody(request);
            Record record(
                requiredString(body, "firstName"),
                requiredString(body, "lastName"),
                body.value("dateOfBirth", ""),
                body.value("gender", "Not specified"),
                body.value("notes", ""));
            Session session = auth.session(token);
            sendJson(response, 201, records.create(record, session.userId));
        } catch (const ValidationException& error) {
            sendJson(response, 400, json{{"error", error.what()}});
        } catch (const std::exception&) {
            sendJson(response, 500,
                     json{{"error", "Record could not be created"}});
        }
    });

    server.Put("/api/records/:id", [&records, &auth](const httplib::Request& request,
                                                     httplib::Response& response) {
        try {
            std::string token = bearerToken(request);
            if (!auth.hasPermission(token))
                throw ValidationException("Authentication required");
            json body = parseBody(request);
            Record record(
                requiredString(body, "firstName"),
                requiredString(body, "lastName"),
                body.value("dateOfBirth", ""),
                body.value("gender", "Not specified"),
                body.value("notes", ""));
            Session session = auth.session(token);
            sendJson(response, 200,
                     records.update(positiveId(request.path_params.at("id")),
                                    record, session.userId));
        } catch (const ValidationException& error) {
            sendJson(response, 400, json{{"error", error.what()}});
        } catch (const std::exception&) {
            sendJson(response, 500,
                     json{{"error", "Record could not be updated"}});
        }
    });

    server.Post("/api/records/:id/archive", [&records, &auth]
                (const httplib::Request& request, httplib::Response& response) {
        try {
            std::string token = bearerToken(request);
            if (!auth.hasPermission(token))
                throw ValidationException("Authentication required");
            Session session = auth.session(token);
            sendJson(response, 200,
                     records.archive(positiveId(request.path_params.at("id")),
                                     session.userId));
        } catch (const ValidationException& error) {
            sendJson(response, 404, json{{"error", error.what()}});
        } catch (const std::exception&) {
            sendJson(response, 500,
                     json{{"error", "Record could not be archived"}});
        }
    });

    server.Delete("/api/records/:id", [&records, &auth]
                  (const httplib::Request& request, httplib::Response& response) {
        std::string token = bearerToken(request);
        if (!auth.hasPermission(token, "Administrator")) {
            sendJson(response, 403,
                     json{{"error", "Administrator permission required"}});
            return;
        }
        try {
            Session session = auth.session(token);
            sendJson(response, 200,
                     json{{"deleted",
                           records.remove(positiveId(request.path_params.at("id")),
                                          session.userId)}});
        } catch (const ValidationException& error) {
            sendJson(response, 404, json{{"error", error.what()}});
        } catch (const std::exception&) {
            sendJson(response, 500,
                     json{{"error", "Record could not be deleted"}});
        }
    });

    server.Get("/api/reports", [&report, &auth](const httplib::Request& request,
                                                httplib::Response& response) {
        try {
            if (!auth.hasPermission(bearerToken(request)))
                throw ValidationException("Authentication required");
            std::string from = request.has_param("from")
                                   ? request.get_param_value("from") : "";
            std::string to = request.has_param("to")
                                 ? request.get_param_value("to") : "";
            sendJson(response, 200, report.generate(from, to));
        } catch (const ValidationException& error) {
            sendJson(response, 400, json{{"error", error.what()}});
        } catch (const std::exception&) {
            sendJson(response, 500,
                     json{{"error", "Report could not be generated"}});
        }
    });

    server.Get("/api/users", [&auth](const httplib::Request& request,
                                     httplib::Response& response) {
        try {
            sendJson(response, 200,
                     json{{"items", auth.listUsers(bearerToken(request))}});
        } catch (const ValidationException& error) {
            sendJson(response, 403, json{{"error", error.what()}});
        } catch (const std::exception&) {
            sendJson(response, 500,
                     json{{"error", "Users could not be loaded"}});
        }
    });

    server.Post("/api/users", [&auth](const httplib::Request& request,
                                      httplib::Response& response) {
        try {
            json body = parseBody(request);
            json user = auth.createUser(
                bearerToken(request),
                requiredString(body, "name"),
                requiredString(body, "username"),
                requiredString(body, "password"),
                requiredString(body, "role"));
            sendJson(response, 201, user);
        } catch (const ValidationException& error) {
            sendJson(response, 400, json{{"error", error.what()}});
        } catch (const std::exception&) {
            sendJson(response, 500,
                     json{{"error", "User could not be created"}});
        }
    });
}

}  // namespace

int main() {
    try {
        Database database(databasePath());
        database.initialize();

        AuthService auth(database);
        auth.seedDemoData();
        RecordService records(database);
        Report report(database);
        Manager manager(database, auth, records, report);

        httplib::Server server;
        server.set_pre_routing_handler(
            [](const httplib::Request& request, httplib::Response& response) {
                response.set_header(
                    "Access-Control-Allow-Origin",
                    "https://orphanage-record-system.vercel.app");
                response.set_header("Access-Control-Allow-Headers",
                                    "Authorization, Content-Type");
                response.set_header("Access-Control-Allow-Methods",
                                    "GET, POST, PUT, DELETE, OPTIONS");
                if (request.method == "OPTIONS") {
                    response.status = 204;
                    return httplib::Server::HandlerResponse::Handled;
                }
                return httplib::Server::HandlerResponse::Unhandled;
            });
        installRoutes(server, database, auth, records, report, manager);

        int port = portFromEnvironment();
        std::cout << "Orphanage Record System running at http://localhost:"
                  << port << "\n";
        server.listen("0.0.0.0", port);
    } catch (const std::exception& error) {
        std::cerr << "Startup failed: " << error.what() << "\n";
        return 1;
    }
    return 0;
}
