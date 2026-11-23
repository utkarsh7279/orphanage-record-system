#pragma once

#include <stdexcept>
#include <string>

class ValidationException : public std::runtime_error {
public:
    explicit ValidationException(const std::string& message)
        : std::runtime_error(message) {}
};

class User {
public:
    User(int id, std::string name, std::string username, std::string role)
        : id_(id), name_(std::move(name)), username_(std::move(username)),
          role_(std::move(role)) {}

    virtual ~User() = default;
    int id() const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& username() const { return username_; }
    const std::string& role() const { return role_; }

private:
    int id_;
    std::string name_;
    std::string username_;
    std::string role_;
};

class StaffUser : public User {
public:
    StaffUser(int id, std::string name, std::string username)
        : User(id, std::move(name), std::move(username), "Staff") {}
};

class Administrator : public User {
public:
    Administrator(int id, std::string name, std::string username)
        : User(id, std::move(name), std::move(username), "Administrator") {}
};

class Record {
public:
    Record(std::string firstName, std::string lastName, std::string dateOfBirth,
           std::string gender, std::string notes)
        : firstName_(std::move(firstName)), lastName_(std::move(lastName)),
          dateOfBirth_(std::move(dateOfBirth)), gender_(std::move(gender)),
          notes_(std::move(notes)) {
        validate();
    }

    const std::string& firstName() const { return firstName_; }
    const std::string& lastName() const { return lastName_; }
    const std::string& dateOfBirth() const { return dateOfBirth_; }
    const std::string& gender() const { return gender_; }
    const std::string& notes() const { return notes_; }

private:
    void validate() const {
        if (firstName_.empty() || lastName_.empty())
            throw ValidationException("First name and last name are required");
        if (firstName_.size() > 80 || lastName_.size() > 80)
            throw ValidationException("Names must be 80 characters or fewer");
        if (notes_.size() > 2000)
            throw ValidationException("Notes must be 2000 characters or fewer");
        if (gender_ != "Female" && gender_ != "Male" &&
            gender_ != "Other" && gender_ != "Not specified")
            throw ValidationException("Invalid gender value");
        if (!dateOfBirth_.empty()) {
            if (dateOfBirth_.size() != 10 ||
                dateOfBirth_[4] != '-' || dateOfBirth_[7] != '-')
                throw ValidationException("Date of birth must use YYYY-MM-DD format");
            for (size_t i = 0; i < dateOfBirth_.size(); ++i) {
                if (i == 4 || i == 7) continue;
                if (dateOfBirth_[i] < '0' || dateOfBirth_[i] > '9')
                    throw ValidationException("Date of birth must use YYYY-MM-DD format");
            }
            int year = std::stoi(dateOfBirth_.substr(0, 4));
            int month = std::stoi(dateOfBirth_.substr(5, 2));
            int day = std::stoi(dateOfBirth_.substr(8, 2));
            if (month < 1 || month > 12)
                throw ValidationException("Date of birth is not valid");
            int maximum = 31;
            if (month == 2) {
                bool leap = year % 400 == 0 ||
                    (year % 4 == 0 && year % 100 != 0);
                maximum = leap ? 29 : 28;
            } else if (month == 4 || month == 6 ||
                       month == 9 || month == 11) {
                maximum = 30;
            }
            if (day < 1 || day > maximum)
                throw ValidationException("Date of birth is not valid");
        }
    }

    std::string firstName_;
    std::string lastName_;
    std::string dateOfBirth_;
    std::string gender_;
    std::string notes_;
};
