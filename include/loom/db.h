#pragma once

#include "loom/status.h"

#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;

namespace loom {

struct Conversation {
    std::string id;
    std::string title;
    std::string created;
    std::string updated;
    std::string source{"user"};
    std::string metadata_json{"{}"};
};

struct Message {
    std::string id;
    std::string conv_id;
    std::optional<std::string> parent_id;
    std::string role;
    std::string text;
    std::optional<std::string> model;
    std::string status{"active"};
    std::optional<std::string> version_group_id;
    int version_num{1};
    double weight{1.0};
    std::string attachments_json{"[]"};
    std::string metadata_json{"{}"};
    std::string created;
    std::string semantic_status{"pending"};
};

struct Node {
    std::string id;
    std::string kind{"entity"};
    std::string label;
    std::string content;
    std::string tags_json{"[]"};
    std::string metadata_json{"{}"};
    std::string created;
};

struct Link {
    std::string id;
    std::string src;
    std::string dst;
    std::string link_type{"related"};
    double weight{1.0};
    std::string metadata_json{"{}"};
    std::string created;
};

struct NewMessage {
    std::string role{"user"};
    std::string text;
    std::optional<std::string> model;
    std::optional<std::string> parent_id;
    double weight{1.0};
    std::string attachments_json{"[]"};
    std::string metadata_json{"{}"};
};

class Database {
public:
    static constexpr int kSchemaVersion = 4;

    Database();
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&&) = delete;
    Database& operator=(Database&&) = delete;

    [[nodiscard]] Status open(const std::string& path);
    void close();
    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] Result<int> schema_version() const;

    [[nodiscard]] Result<Conversation> create_conversation(std::string_view title = "New Chat");
    [[nodiscard]] Result<std::optional<Conversation>> get_conversation(std::string_view id) const;
    [[nodiscard]] Result<std::vector<Conversation>> list_conversations(int limit = 50) const;
    [[nodiscard]] Status update_conversation_title(std::string_view id, std::string_view title);
    [[nodiscard]] Status delete_conversation(std::string_view id);

    [[nodiscard]] Result<std::string> create_message(
        std::string_view conv_id,
        std::string_view text,
        std::string_view role,
        std::optional<std::string> model = std::nullopt,
        std::optional<std::string> parent_id = std::nullopt,
        std::string attachments_json = "[]",
        std::optional<std::string> version_group_id = std::nullopt,
        double weight = 1.0,
        std::string metadata_json = "{}");
    [[nodiscard]] Result<std::size_t> batch_create_messages(
        std::string_view conv_id,
        const std::vector<NewMessage>& messages,
        std::size_t batch_size = 1000);
    [[nodiscard]] Result<std::optional<Message>> get_message(std::string_view id) const;
    [[nodiscard]] Result<std::vector<Message>> get_messages(
        std::string_view conv_id, bool include_all = false) const;
    [[nodiscard]] Result<std::vector<Message>> get_versions(std::string_view version_group_id) const;
    [[nodiscard]] Result<std::string> edit_message(std::string_view id, std::string_view new_text);
    [[nodiscard]] Status restore_version(std::string_view id);
    [[nodiscard]] Status set_message_status(std::string_view id, std::string_view status);
    [[nodiscard]] Result<std::vector<Message>> get_unanalysed_messages(int limit = 100) const;
    [[nodiscard]] Result<std::size_t> count_pending_semantic() const;
    [[nodiscard]] Status mark_analysed(std::string_view id, std::string_view metadata_json);

    [[nodiscard]] Result<std::string> create_node(
        std::string_view label,
        std::string_view kind = "entity",
        std::string_view content = "",
        std::string tags_json = "[]",
        std::string metadata_json = "{}",
        std::optional<std::string> node_id = std::nullopt);
    [[nodiscard]] Result<std::optional<Node>> get_node(std::string_view id) const;
    [[nodiscard]] Result<std::optional<Node>> find_node(
        std::string_view label,
        std::optional<std::string> kind = std::nullopt) const;
    [[nodiscard]] Result<std::string> get_or_create_node(
        std::string_view label, std::string_view kind = "entity");
    [[nodiscard]] Result<std::vector<Node>> list_nodes(
        std::optional<std::string> kind = std::nullopt, int limit = 200) const;
    [[nodiscard]] Status delete_node(std::string_view id);

    [[nodiscard]] Result<std::string> create_link(
        std::string_view src,
        std::string_view dst,
        std::string_view link_type = "related",
        double weight = 1.0,
        std::string metadata_json = "{}");
    [[nodiscard]] Result<std::vector<Link>> get_links(
        std::optional<std::string> node_id = std::nullopt,
        std::optional<std::string> link_type = std::nullopt) const;
    [[nodiscard]] Status delete_link(std::string_view id);

private:
    [[nodiscard]] Status init_schema_unlocked();
    [[nodiscard]] Status migrate_unlocked();
    [[nodiscard]] Status exec_unlocked(std::string_view sql) const;
    [[nodiscard]] bool has_table_unlocked(std::string_view table) const;
    [[nodiscard]] bool has_column_unlocked(std::string_view table, std::string_view column) const;
    [[nodiscard]] Status ensure_column_unlocked(
        std::string_view table, std::string_view column, std::string_view ddl) const;
    [[nodiscard]] Status set_meta_unlocked(std::string_view key, std::string_view value) const;
    [[nodiscard]] Result<std::optional<std::string>> get_meta_unlocked(std::string_view key) const;
    [[nodiscard]] Status touch_conversation_unlocked(std::string_view id) const;

    sqlite3* db_{nullptr};
    mutable std::mutex mutex_;
};

}  // namespace loom
