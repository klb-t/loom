#include "loom/db.h"
#include "db_internal.h"

#include <charconv>

namespace loom {
using namespace db_internal;

Database::Database() = default;
Database::~Database() { close(); }

Status Database::open(const std::string& path) {
    std::lock_guard lock(mutex_);
    if (db_) return {StatusCode::already_exists, "database already open"};

    sqlite3* candidate = nullptr;
    const int rc = sqlite3_open_v2(
        path.c_str(), &candidate,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr);
    if (rc != SQLITE_OK) {
        const auto st = sqlite_status(candidate, "open database", rc);
        if (candidate) sqlite3_close(candidate);
        return st;
    }
    db_ = candidate;
    sqlite3_busy_timeout(db_, 30000);

    for (const auto* pragma : {
             "PRAGMA journal_mode=WAL",
             "PRAGMA synchronous=NORMAL",
             "PRAGMA foreign_keys=ON"}) {
        const auto st = exec_unlocked(pragma);
        if (!st.ok()) {
            sqlite3_close(db_);
            db_ = nullptr;
            return st;
        }
    }
    auto st = init_schema_unlocked();
    if (!st.ok()) {
        sqlite3_close(db_); db_ = nullptr; return st;
    }
    st = migrate_unlocked();
    if (!st.ok()) {
        sqlite3_close(db_); db_ = nullptr; return st;
    }
    return Status::Ok();
}

void Database::close() {
    std::lock_guard lock(mutex_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::is_open() const noexcept {
    std::lock_guard lock(mutex_);
    return db_ != nullptr;
}

Status Database::exec_unlocked(std::string_view sql) const {
    if (!db_) return {StatusCode::unavailable, "database not open"};
    const std::string owned(sql);
    char* err = nullptr;
    const int rc = sqlite3_exec(db_, owned.c_str(), nullptr, nullptr, &err);
    if (rc == SQLITE_OK) return Status::Ok();
    std::string msg = err ? err : sqlite3_errmsg(db_);
    sqlite3_free(err);
    return {StatusCode::database_error, std::move(msg)};
}

Status Database::init_schema_unlocked() {
    return exec_unlocked(R"SQL(
CREATE TABLE IF NOT EXISTS _meta (key TEXT PRIMARY KEY, value TEXT);
CREATE TABLE IF NOT EXISTS conversations (
    id TEXT PRIMARY KEY, title TEXT NOT NULL,
    created TEXT NOT NULL, updated TEXT NOT NULL,
    source TEXT NOT NULL DEFAULT 'user',
    metadata TEXT NOT NULL DEFAULT '{}'
);
CREATE TABLE IF NOT EXISTS messages (
    id TEXT PRIMARY KEY, conv_id TEXT NOT NULL,
    parent_id TEXT, role TEXT NOT NULL, text TEXT NOT NULL,
    model TEXT, status TEXT NOT NULL DEFAULT 'active',
    version_group_id TEXT, version_num INTEGER NOT NULL DEFAULT 1,
    weight REAL NOT NULL DEFAULT 1.0,
    attachments TEXT NOT NULL DEFAULT '[]',
    metadata TEXT NOT NULL DEFAULT '{}',
    created TEXT NOT NULL,
    semantic_status TEXT NOT NULL DEFAULT 'pending',
    FOREIGN KEY (conv_id) REFERENCES conversations(id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS nodes (
    id TEXT PRIMARY KEY, kind TEXT NOT NULL DEFAULT 'entity',
    label TEXT NOT NULL, content TEXT NOT NULL DEFAULT '',
    tags TEXT NOT NULL DEFAULT '[]',
    metadata TEXT NOT NULL DEFAULT '{}',
    created TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS links (
    id TEXT PRIMARY KEY, src TEXT NOT NULL, dst TEXT NOT NULL,
    link_type TEXT NOT NULL DEFAULT 'related',
    weight REAL NOT NULL DEFAULT 1.0,
    metadata TEXT NOT NULL DEFAULT '{}',
    created TEXT NOT NULL
);
)SQL");
}

bool Database::has_table_unlocked(std::string_view table) const {
    Statement s(db_, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?");
    if (!s.ok() || !bind_text(db_, s.get(), 1, table).ok()) return false;
    return sqlite3_step(s.get()) == SQLITE_ROW;
}

bool Database::has_column_unlocked(std::string_view table, std::string_view column) const {
    Statement s(db_, "PRAGMA table_info(" + std::string(table) + ")");
    if (!s.ok()) return false;
    while (sqlite3_step(s.get()) == SQLITE_ROW) {
        if (col_text(s.get(), 1) == column) return true;
    }
    return false;
}

Status Database::ensure_column_unlocked(std::string_view table, std::string_view column,
                                        std::string_view ddl) const {
    if (has_column_unlocked(table, column)) return Status::Ok();
    return exec_unlocked(ddl);
}

Status Database::set_meta_unlocked(std::string_view key, std::string_view value) const {
    Statement s(db_, "INSERT OR REPLACE INTO _meta(key,value) VALUES(?,?)");
    if (!s.ok()) return sqlite_status(db_, "prepare set meta", s.rc());
    if (auto st = bind_text(db_, s.get(), 1, key); !st.ok()) return st;
    if (auto st = bind_text(db_, s.get(), 2, value); !st.ok()) return st;
    const int rc = sqlite3_step(s.get());
    return rc == SQLITE_DONE ? Status::Ok() : sqlite_status(db_, "set meta", rc);
}

Result<std::optional<std::string>> Database::get_meta_unlocked(std::string_view key) const {
    Statement s(db_, "SELECT value FROM _meta WHERE key=?");
    if (!s.ok()) return sqlite_status(db_, "prepare get meta", s.rc());
    if (auto st = bind_text(db_, s.get(), 1, key); !st.ok()) return st;
    const int rc = sqlite3_step(s.get());
    if (rc == SQLITE_DONE) return std::optional<std::string>{};
    if (rc != SQLITE_ROW) return sqlite_status(db_, "get meta", rc);
    return std::optional<std::string>{col_text(s.get(), 0)};
}

Status Database::migrate_unlocked() {
    struct Col { const char* table; const char* col; const char* ddl; };
    const Col cols[] = {
        {"conversations", "source", "ALTER TABLE conversations ADD COLUMN source TEXT NOT NULL DEFAULT 'user'"},
        {"conversations", "metadata", "ALTER TABLE conversations ADD COLUMN metadata TEXT NOT NULL DEFAULT '{}'"},
        {"messages", "parent_id", "ALTER TABLE messages ADD COLUMN parent_id TEXT"},
        {"messages", "model", "ALTER TABLE messages ADD COLUMN model TEXT"},
        {"messages", "status", "ALTER TABLE messages ADD COLUMN status TEXT NOT NULL DEFAULT 'active'"},
        {"messages", "version_group_id", "ALTER TABLE messages ADD COLUMN version_group_id TEXT"},
        {"messages", "version_num", "ALTER TABLE messages ADD COLUMN version_num INTEGER NOT NULL DEFAULT 1"},
        {"messages", "weight", "ALTER TABLE messages ADD COLUMN weight REAL NOT NULL DEFAULT 1.0"},
        {"messages", "attachments", "ALTER TABLE messages ADD COLUMN attachments TEXT NOT NULL DEFAULT '[]'"},
        {"messages", "metadata", "ALTER TABLE messages ADD COLUMN metadata TEXT NOT NULL DEFAULT '{}'"},
        {"messages", "semantic_status", "ALTER TABLE messages ADD COLUMN semantic_status TEXT NOT NULL DEFAULT 'pending'"},
        {"nodes", "kind", "ALTER TABLE nodes ADD COLUMN kind TEXT NOT NULL DEFAULT 'entity'"},
        {"nodes", "content", "ALTER TABLE nodes ADD COLUMN content TEXT NOT NULL DEFAULT ''"},
        {"nodes", "tags", "ALTER TABLE nodes ADD COLUMN tags TEXT NOT NULL DEFAULT '[]'"},
        {"nodes", "metadata", "ALTER TABLE nodes ADD COLUMN metadata TEXT NOT NULL DEFAULT '{}'"},
        {"links", "link_type", "ALTER TABLE links ADD COLUMN link_type TEXT NOT NULL DEFAULT 'related'"},
        {"links", "weight", "ALTER TABLE links ADD COLUMN weight REAL NOT NULL DEFAULT 1.0"},
        {"links", "metadata", "ALTER TABLE links ADD COLUMN metadata TEXT NOT NULL DEFAULT '{}'"},
    };
    for (const auto& c : cols) {
        if (auto st = ensure_column_unlocked(c.table, c.col, c.ddl); !st.ok()) return st;
    }

    const char* indexes[] = {
        "CREATE INDEX IF NOT EXISTS idx_msg_conv ON messages(conv_id)",
        "CREATE INDEX IF NOT EXISTS idx_msg_parent ON messages(parent_id)",
        "CREATE INDEX IF NOT EXISTS idx_msg_vgroup ON messages(version_group_id)",
        "CREATE INDEX IF NOT EXISTS idx_msg_status ON messages(conv_id, status)",
        "CREATE INDEX IF NOT EXISTS idx_msg_semantic ON messages(semantic_status)",
        "CREATE INDEX IF NOT EXISTS idx_nodes_kind ON nodes(kind)",
        "CREATE INDEX IF NOT EXISTS idx_links_src ON links(src)",
        "CREATE INDEX IF NOT EXISTS idx_links_dst ON links(dst)",
        "CREATE INDEX IF NOT EXISTS idx_links_type ON links(link_type)",
    };
    for (const auto* sql : indexes) {
        if (auto st = exec_unlocked(sql); !st.ok()) return st;
    }
    return set_meta_unlocked("schema_version", "4");
}

Result<int> Database::schema_version() const {
    std::lock_guard lock(mutex_);
    if (!db_) return Status{StatusCode::unavailable, "database not open"};
    auto value = get_meta_unlocked("schema_version");
    if (!value.ok()) return value.status();
    if (!value.value()) return 0;
    int parsed = 0;
    const auto& text = *value.value();
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (ec != std::errc{} || ptr != text.data() + text.size()) {
        return Status{StatusCode::parse_error, "invalid schema_version"};
    }
    return parsed;
}

Status Database::touch_conversation_unlocked(std::string_view id) const {
    Statement s(db_, "UPDATE conversations SET updated=? WHERE id=?");
    if (!s.ok()) return sqlite_status(db_, "prepare touch conversation", s.rc());
    const auto now = now_iso8601_utc();
    if (auto st = bind_text(db_, s.get(), 1, now); !st.ok()) return st;
    if (auto st = bind_text(db_, s.get(), 2, id); !st.ok()) return st;
    const int rc = sqlite3_step(s.get());
    return rc == SQLITE_DONE ? Status::Ok() : sqlite_status(db_, "touch conversation", rc);
}

Result<Conversation> Database::create_conversation(std::string_view title) {
    std::lock_guard lock(mutex_);
    if (!db_) return Status{StatusCode::unavailable, "database not open"};
    Conversation c;
    c.id = make_id("c_");
    c.title = std::string(title);
    c.created = c.updated = now_iso8601_utc();

    Statement s(db_, "INSERT INTO conversations(id,title,created,updated,source,metadata) VALUES(?,?,?,?,?,?)");
    if (!s.ok()) return sqlite_status(db_, "prepare create conversation", s.rc());
    const std::string_view vals[] = {c.id, c.title, c.created, c.updated, c.source, c.metadata_json};
    for (int i = 0; i < 6; ++i) if (auto st = bind_text(db_, s.get(), i+1, vals[i]); !st.ok()) return st;
    const int rc = sqlite3_step(s.get());
    if (rc != SQLITE_DONE) return sqlite_status(db_, "create conversation", rc);
    return c;
}

Result<std::optional<Conversation>> Database::get_conversation(std::string_view id) const {
    std::lock_guard lock(mutex_);
    if (!db_) return Status{StatusCode::unavailable, "database not open"};
    Statement s(db_, "SELECT id,title,created,updated,source,metadata FROM conversations WHERE id=?");
    if (!s.ok()) return sqlite_status(db_, "prepare get conversation", s.rc());
    if (auto st = bind_text(db_, s.get(), 1, id); !st.ok()) return st;
    const int rc = sqlite3_step(s.get());
    if (rc == SQLITE_DONE) return std::optional<Conversation>{};
    if (rc != SQLITE_ROW) return sqlite_status(db_, "get conversation", rc);
    return std::optional<Conversation>{read_conversation(s.get())};
}

Result<std::vector<Conversation>> Database::list_conversations(int limit) const {
    std::lock_guard lock(mutex_);
    if (!db_) return Status{StatusCode::unavailable, "database not open"};
    if (limit <= 0) return Status{StatusCode::invalid_argument, "limit must be > 0"};
    Statement s(db_, "SELECT id,title,created,updated,source,metadata FROM conversations ORDER BY updated DESC LIMIT ?");
    if (!s.ok()) return sqlite_status(db_, "prepare list conversations", s.rc());
    sqlite3_bind_int(s.get(), 1, limit);
    std::vector<Conversation> out;
    for (;;) {
        const int rc = sqlite3_step(s.get());
        if (rc == SQLITE_DONE) break;
        if (rc != SQLITE_ROW) return sqlite_status(db_, "list conversations", rc);
        out.push_back(read_conversation(s.get()));
    }
    return out;
}

Status Database::update_conversation_title(std::string_view id, std::string_view title) {
    std::lock_guard lock(mutex_);
    if (!db_) return {StatusCode::unavailable, "database not open"};
    Statement s(db_, "UPDATE conversations SET title=?,updated=? WHERE id=?");
    if (!s.ok()) return sqlite_status(db_, "prepare update conversation", s.rc());
    const auto now = now_iso8601_utc();
    if (auto st=bind_text(db_,s.get(),1,title);!st.ok()) return st;
    if (auto st=bind_text(db_,s.get(),2,now);!st.ok()) return st;
    if (auto st=bind_text(db_,s.get(),3,id);!st.ok()) return st;
    const int rc=sqlite3_step(s.get());
    return rc==SQLITE_DONE ? Status::Ok() : sqlite_status(db_,"update conversation",rc);
}

Status Database::delete_conversation(std::string_view id) {
    std::lock_guard lock(mutex_);
    if (!db_) return {StatusCode::unavailable, "database not open"};
    auto st = begin(db_); if (!st.ok()) return st;
    for (const auto* sql : {"DELETE FROM messages WHERE conv_id=?", "DELETE FROM conversations WHERE id=?"}) {
        Statement s(db_, sql);
        if (!s.ok()) { rollback(db_); return sqlite_status(db_, "prepare delete conversation", s.rc()); }
        if (auto bst=bind_text(db_,s.get(),1,id);!bst.ok()) { rollback(db_); return bst; }
        if (sqlite3_step(s.get()) != SQLITE_DONE) { auto e=sqlite_status(db_,"delete conversation"); rollback(db_); return e; }
    }
    st = commit(db_); if (!st.ok()) rollback(db_); return st;
}

}  // namespace loom
