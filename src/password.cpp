#include "password.h"

#include <openssl/rand.h>
#include <openssl/evp.h>

#include <iomanip>
#include <sstream>
#include <vector>

namespace {
std::string to_hex(const unsigned char* data, size_t size) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (size_t i = 0; i < size; ++i)
        out << std::setw(2) << static_cast<unsigned int>(data[i]);
    return out.str();
}

std::vector<unsigned char> from_hex(const std::string& text) {
    std::vector<unsigned char> bytes;
    if (text.size() % 2 != 0) return bytes;
    for (size_t i = 0; i < text.size(); i += 2) {
        unsigned int value;
        std::istringstream(text.substr(i, 2)) >> std::hex >> value;
        bytes.push_back(static_cast<unsigned char>(value));
    }
    return bytes;
}
}

std::string PasswordHash::hash(const std::string& password) {
    constexpr int iterations = 600000;
    constexpr size_t saltSize = 16;
    constexpr size_t hashSize = 32;
    unsigned char salt[saltSize];
    unsigned char derived[hashSize];
    if (RAND_bytes(salt, static_cast<int>(saltSize)) != 1)
        throw std::runtime_error("Unable to generate password salt");
    if (PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()),
                           salt, static_cast<int>(saltSize), iterations,
                           EVP_sha256(), static_cast<int>(hashSize), derived) != 1)
        throw std::runtime_error("Unable to hash password");

    return "pbkdf2-sha256$" + std::to_string(iterations) + "$" +
           to_hex(salt, saltSize) + "$" + to_hex(derived, hashSize);
}

bool PasswordHash::verify(const std::string& password, const std::string& encoded) {
    size_t first = encoded.find('$');
    size_t second = encoded.find('$', first + 1);
    size_t third = encoded.find('$', second + 1);
    if (first == std::string::npos || second == std::string::npos ||
        third == std::string::npos) return false;
    if (encoded.substr(0, first) != "pbkdf2-sha256") return false;

    try {
        int iterations = std::stoi(encoded.substr(first + 1, second - first - 1));
        auto salt = from_hex(encoded.substr(second + 1, third - second - 1));
        auto expected = from_hex(encoded.substr(third + 1));
        if (salt.empty() || expected.empty()) return false;
        std::vector<unsigned char> actual(expected.size());
        if (PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()),
                               salt.data(), static_cast<int>(salt.size()), iterations,
                               EVP_sha256(), static_cast<int>(actual.size()),
                               actual.data()) != 1) return false;

        unsigned char difference = 0;
        for (size_t i = 0; i < actual.size(); ++i)
            difference |= actual[i] ^ expected[i];
        return difference == 0;
    } catch (...) {
        return false;
    }
}
