#pragma once

// A tiny Result<T> / Result<void> type used across bedrocklua so that fallible
// operations (signature resolution, manifest parsing, file IO) can report a
// human-readable reason without throwing across ABI boundaries.

#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace bedrocklua {

struct Error {
    std::string message;
};

template <typename T>
class Result {
public:
    Result(T value) : data_(std::move(value)) {}
    Result(Error error) : data_(std::move(error)) {}

    static Result fail(std::string message) { return Result(Error{std::move(message)}); }

    bool ok() const { return std::holds_alternative<T>(data_); }
    explicit operator bool() const { return ok(); }

    T& value() { return std::get<T>(data_); }
    const T& value() const { return std::get<T>(data_); }

    T value_or(T fallback) const { return ok() ? std::get<T>(data_) : std::move(fallback); }

    const std::string& error() const { return std::get<Error>(data_).message; }

private:
    std::variant<T, Error> data_;
};

// Specialisation for operations that only succeed or fail.
template <>
class Result<void> {
public:
    Result() : error_(std::nullopt) {}
    Result(Error error) : error_(std::move(error)) {}

    static Result fail(std::string message) { return Result(Error{std::move(message)}); }
    static Result success() { return Result(); }

    bool ok() const { return !error_.has_value(); }
    explicit operator bool() const { return ok(); }

    const std::string& error() const { return error_->message; }

private:
    std::optional<Error> error_;
};

}  // namespace bedrocklua
