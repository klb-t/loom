#include "loom/db.h"

#include <sqlite3.h>

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static fs::path temp_db(const char* name) {
    auto p = fs::temp_directory_path() / name;
    std::error_code ec;
    fs::remove(p, ec);
    fs::remove(p.string() + "-wal", ec);
    fs::remove(p.string() + "-shm", ec);
    return p;
}

static void make_legacy_db(const fs::path& p) {
    sqlite3* db = nullptr;
    assert(sqlite3_open(p.string().c_str(), &db) == SQLITE_OK);
    const char* sql = R"SQL(
CREATE TABLE _meta (key TEXT PRIMARY KEY, value TEXT);
INSERT INTO _meta(key,value) VALUES('schema_version','0');
CREATE TABLE conversations (
  id TEXT PRIMARY KEY, title TEXT NOT NULL, created TEXT NOT NULL,
  updated TEXT NOT NULL, metadata TEXT NOT NULL DEFAULT '{}'
);
CREATE TABLE messages (
  id TEXT PRIMARY KEY, conv_id TEXT NOT NULL, role TEXT NOT NULL,
  text TEXT NOT NULL, created TEXT NOT NULL
);
CREATE TABLE nodes (
  id TEXT PRIMARY KEY, label TEXT NOT NULL, created TEXT NOT NULL
);
CREATE TABLE links (
  id TEXT PRIMARY KEY, src TEXT NOT NULL, dst TEXT NOT NULL,
  created TEXT NOT NULL
);
)SQL";
    assert(sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK);
    sqlite3_close(db);
}

int main() {
    {
        const auto p = temp_db("loom_fresh_compat_test.db");
        loom::Database db;
        auto st = db.open(p.string());
        assert(st.ok());
        auto ver = db.schema_version();
        assert(ver.ok() && ver.value() == 4);

        auto conv = db.create_conversation("compat");
        assert(conv.ok());
        assert(conv.value().id.rfind("c_", 0) == 0);

        auto m1 = db.create_message(conv.value().id, "first version long enough", "user");
        assert(m1.ok());
        auto before = db.get_message(m1.value());
        assert(before.ok() && before.value());
        assert(before.value()->semantic_status == "pending");

        auto m2 = db.edit_message(m1.value(), "second version long enough");
        assert(m2.ok());
        auto edited = db.get_message(m2.value());
        assert(edited.ok() && edited.value());
        auto versions = db.get_versions(*edited.value()->version_group_id);
        assert(versions.ok() && versions.value().size() == 2);
        assert(versions.value()[0].status == "version");
        assert(versions.value()[1].status == "active");

        std::vector<loom::NewMessage> batch{
            {.role="user", .text="a batch message long enough"},
            {.role="assistant", .text="another batch message long enough"},
        };
        auto inserted = db.batch_create_messages(conv.value().id, batch);
        assert(inserted.ok() && inserted.value() == 2);

        auto pending = db.count_pending_semantic();
        assert(pending.ok() && pending.value() == 4);

        auto n1 = db.get_or_create_node("ChatADHD", "project");
        auto n2 = db.get_or_create_node("Loom", "project");
        assert(n1.ok() && n2.ok());
        auto link = db.create_link(n1.value(), n2.value(), "implements", 0.8);
        assert(link.ok());
        auto links = db.get_links(n1.value());
        assert(links.ok() && links.value().size() == 1);

        db.close();
        std::error_code ec;
        fs::remove(p, ec);
        fs::remove(p.string() + "-wal", ec);
        fs::remove(p.string() + "-shm", ec);
    }

    {
        const auto p = temp_db("loom_legacy_compat_test.db");
        make_legacy_db(p);
        loom::Database db;
        auto st = db.open(p.string());
        assert(st.ok());
        auto ver = db.schema_version();
        assert(ver.ok() && ver.value() == 4);

        auto conv = db.create_conversation("after migration");
        assert(conv.ok());
        auto msg = db.create_message(conv.value().id, "migration preserved write path", "user");
        assert(msg.ok());

        db.close();
        std::error_code ec;
        fs::remove(p, ec);
        fs::remove(p.string() + "-wal", ec);
        fs::remove(p.string() + "-shm", ec);
    }

    std::cout << "db_compat_test: OK\n";
    return 0;
}
