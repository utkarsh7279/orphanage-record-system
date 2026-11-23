cmake_minimum_required(VERSION 3.20)
project(OrphanageRecordSystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.18.3
)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(httplib nlohmann_json)

find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(orphanage_system
    src/main.cpp
    src/database.cpp
    src/password.cpp
    src/services.cpp
)

target_include_directories(orphanage_system PRIVATE src)
target_link_libraries(orphanage_system
    PRIVATE
        httplib
        nlohmann_json::nlohmann_json
        SQLite::SQLite3
        OpenSSL::Crypto
)

enable_testing()

add_executable(ors_tests
    tests/ors_tests.cpp
    src/database.cpp
    src/password.cpp
)

target_include_directories(ors_tests PRIVATE src)
target_link_libraries(ors_tests
    PRIVATE
        SQLite::SQLite3
        OpenSSL::Crypto
)

add_test(NAME ors_tests COMMAND ors_tests)
set_tests_properties(ors_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
