#pragma once

#include "loom/db.h"

#include <sqlite3.h>

#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace loom::db_internal {

class Statement {
public:
    Statement(sqlite3* db, std::string_view sql) : db_(db) {
        const std::string owned(sql);
        rc_ = sqlite3_prepare_v2(db_, owned.c_str(), -1, &stmt_, nullptr);
    }
    ~Statement() {
        if (stmt_) sqlite3_finalize(stmt_);
    }
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    [[nodiscard]] bool ok() const noexcept { return rc_ == SQLITE_OK && stmt_ != nullptr; }
    [[nodiscard]] int rc() const noexcept { return rc_; }
    [[nodiscard]] sqlite3_stmt* get() const noexcept { return stmt_; }

private:
    sqlite3* db_{};
    sqlite3_stmt* stmt_{};
    int rc_{SQLITE_ERROR};
};

inline Status sqlite_status(sqlite3* db, std::string_view prefix, int rc = SQLITE_ERROR) {
    std::string msg(prefix);
    if (db) {
        msg += ": ";
        msg += sqlite3_errmsg(db);
    }
    if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED) {
        return {StatusCode::unavailable, std::move(msg)};
    }
    return {StatusCode::database_error, std::move(msg)};
}

inline Status bind_text(sqlite3* db, sqlite3_stmt* s, int i, std::string_view value) {
    if (sqlite3_bind_text(s, i, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT) != SQLITE_OK) {
        return sqlite_status(db, "bind text");
    }
    return Status::Ok();
}

inline Status bind_optional_text(sqlite3* db, sqlite3_stmt* s, int i,
                                 const std::optional<std::string>& value) {
    if (!value) {
        if (sqlite3_bind_null(s, i) != SQLITE_OK) return sqlite_status(db, "bind null");
        return Status::Ok();
    }
    return bind_text(db, s, i, *value);
}

inline std::optional<std::string> col_optional_text(sqlite3_stmt* s, int col) {
    if (sqlite3_column_type(s, col) == SQLITE_NULL) return std::nullopt;
    const auto* p = reinterpret_cast<const char*>(sqlite3_column_text(s, col));
    return p ? std::optional<std::string>(p) : std::optional<std::string>(std::string{});
}

inline std::string col_text(sqlite3_stmt* s, int col) {
    const auto* p = reinterpret_cast<const char*>(sqlite3_column_text(s, col));
    return p ? std::string(p) : std::string{};
}

inline Conversation read_conversation(sqlite3_stmt* s) {
    return Conversation{
        col_text(s, 0), col_text(s, 1), col_text(s, 2), col_text(s, 3),
        col_text(s, 4), col_text(s, 5)};
}

inline Message read_message(sqlite3_stmt* s) {
    Message m;
    m.id = col_text(s, 0);
    m.conv_id = col_text(s, 1);
    m.parent_id = col_optional_text(s, 2);
    m.role = col_text(s, 3);
    m.text = col_text(s, 4);
    m.model = col_optional_text(s, 5);
    m.status = col_text(s, 6);
    m.version_group_id = col_optional_text(s, 7);
    m.version_num = sqlite3_column_int(s, 8);
    m.weight = sqlite3_column_double(s, 9);
    m.attachments_json = col_text(s, 10);
    m.metadata_json = col_text(s, 11);
    m.created = col_text(s, 12);
    m.semantic_status = col_text(s, 13);
    return m;
}

inline Node read_node(sqlite3_stmt* s) {
    Node n;
    n.id = col_text(s, 0);
    n.kind = col_text(s, 1);
    n.label = col_text(s, 2);
    n.content = col_text(s, 3);
    n.tags_json = col_text(s, 4);
    n.metadata_json = col_text(s, 5);
    n.created = col_text(s, 6);
    return n;
}

inline Link read_link(sqlite3_stmt* s) {
    Link l;
    l.id = col_text(s, 0);
    l.src = col_text(s, 1);
    l.dst = col_text(s, 2);
    l.link_type = col_text(s, 3);
    l.weight = sqlite3_column_double(s, 4);
    l.metadata_json = col_text(s, 5);
    l.created = col_text(s, 6);
    return l;
}

inline std::string now_iso8601_utc() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto us = duration_cast<microseconds>(now.time_since_epoch()) % seconds(1);
    const std::time_t tt = system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S")
        << '.' << std::setw(6) << std::setfill('0') << us.count() << 'Z';
    return out.str();
}

inline std::string random_hex12() {
    thread_local std::mt19937_64 gen{std::random_device{}()};
    const std::uint64_t value = gen();
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(12)
        << (value & 0xFFFFFFFFFFFFULL);
    return out.str();
}

inline std::string make_id(std::string_view prefix) {
    return std::string(prefix) + random_hex12();
}

inline Status begin(sqlite3* db) {
    const int rc = sqlite3_exec(db, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
    return rc == SQLITE_OK ? Status::Ok() : sqlite_status(db, "begin", rc);
}
inline Status commit(sqlite3* db) {
    const int rc = sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr);
    return rc == SQLITE_OK ? Status::Ok() : sqlite_status(db, "commit", rc);
}
inline void rollback(sqlite3* db) {
    sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
}

}  // namespace loom::db_internal
