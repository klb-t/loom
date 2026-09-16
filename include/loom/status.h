#pragma once

#include <optional>
#include <string>
#include <utility>

namespace loom {

enum class StatusCode {
    ok = 0,
    invalid_argument,
    not_found,
    already_exists,
    io_error,
    database_error,
    parse_error,
    unavailable,
    unsupported,
    internal_error,
};

class Status {
public:
    Status() = default;
    Status(StatusCode code, std::string message)
        : code_(code), message_(std::move(message)) {}

    [[nodiscard]] static Status Ok() { return {}; }
    [[nodiscard]] bool ok() const noexcept { return code_ == StatusCode::ok; }
    [[nodiscard]] StatusCode code() const noexcept { return code_; }
    [[nodiscard]] const std::string& message() const noexcept { return message_; }

private:
    StatusCode code_{StatusCode::ok};
    std::string message_{};
};

template <typename T>
class Result {
public:
    Result(T value) : value_(std::move(value)), status_(Status::Ok()) {}
    Result(Status status) : status_(std::move(status)) {}

    [[nodiscard]] bool ok() const noexcept { return status_.ok(); }
    [[nodiscard]] const Status& status() const noexcept { return status_; }
    [[nodiscard]] const T& value() const& { return *value_; }
    [[nodiscard]] T& value() & { return *value_; }
    [[nodiscard]] T&& value() && { return std::move(*value_); }

private:
    std::optional<T> value_{};
    Status status_{};
};

}  // namespace loom
