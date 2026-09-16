#include "loom/db.h"
#include "db_internal.h"

namespace loom {
using namespace db_internal;

Result<std::string> Database::create_message(
    std::string_view conv_id, std::string_view text, std::string_view role,
    std::optional<std::string> model, std::optional<std::string> parent_id,
    std::string attachments_json, std::optional<std::string> version_group_id,
    double weight, std::string metadata_json) {
    std::lock_guard lock(mutex_);
    if (!db_) return Status{StatusCode::unavailable, "database not open"};
    if (text.empty()) return Status{StatusCode::invalid_argument, "message text must not be empty"};

    const std::string mid = make_id("m_");
    const std::string now = now_iso8601_utc();
    std::string vg = version_group_id.value_or(make_id("vg_"));
    int version_num = 1;

    auto st = begin(db_); if (!st.ok()) return st;
    if (version_group_id) {
        Statement q(db_, "SELECT COALESCE(MAX(version_num),0)+1 FROM messages WHERE version_group_id=?");
        if (!q.ok()) { rollback(db_); return sqlite_status(db_,"prepare version query",q.rc()); }
        if (auto bst=bind_text(db_,q.get(),1,vg);!bst.ok()) { rollback(db_); return bst; }
        if (sqlite3_step(q.get())==SQLITE_ROW) version_num=sqlite3_column_int(q.get(),0);
        Statement old(db_, "UPDATE messages SET status='version' WHERE version_group_id=? AND status='active'");
        if (!old.ok()) { rollback(db_); return sqlite_status(db_,"prepare version old",old.rc()); }
        if (auto bst=bind_text(db_,old.get(),1,vg);!bst.ok()) { rollback(db_); return bst; }
        if (sqlite3_step(old.get())!=SQLITE_DONE) { auto e=sqlite_status(db_,"mark old version"); rollback(db_); return e; }
    }

    Statement s(db_, R"SQL(INSERT INTO messages
(id,conv_id,parent_id,role,text,model,status,version_group_id,version_num,weight,attachments,metadata,created,semantic_status)
VALUES(?,?,?,?,?,?,'active',?,?,?,?,?,?,'pending'))SQL");
    if (!s.ok()) { rollback(db_); return sqlite_status(db_,"prepare create message",s.rc()); }
    int i=1;
    if (auto x=bind_text(db_,s.get(),i++,mid);!x.ok()) { rollback(db_); return x; }
    if (auto x=bind_text(db_,s.get(),i++,conv_id);!x.ok()) { rollback(db_); return x; }
    if (auto x=bind_optional_text(db_,s.get(),i++,parent_id);!x.ok()) { rollback(db_); return x; }
    if (auto x=bind_text(db_,s.get(),i++,role);!x.ok()) { rollback(db_); return x; }
    if (auto x=bind_text(db_,s.get(),i++,text);!x.ok()) { rollback(db_); return x; }
    if (auto x=bind_optional_text(db_,s.get(),i++,model);!x.ok()) { rollback(db_); return x; }
    if (auto x=bind_text(db_,s.get(),i++,vg);!x.ok()) { rollback(db_); return x; }
    sqlite3_bind_int(s.get(), i++, version_num);
    sqlite3_bind_double(s.get(), i++, weight);
    if (auto x=bind_text(db_,s.get(),i++,attachments_json);!x.ok()) { rollback(db_); return x; }
    if (auto x=bind_text(db_,s.get(),i++,metadata_json);!x.ok()) { rollback(db_); return x; }
    if (auto x=bind_text(db_,s.get(),i++,now);!x.ok()) { rollback(db_); return x; }
    if (sqlite3_step(s.get())!=SQLITE_DONE) { auto e=sqlite_status(db_,"create message"); rollback(db_); return e; }
    if (auto x=touch_conversation_unlocked(conv_id);!x.ok()) { rollback(db_); return x; }
    st=commit(db_); if (!st.ok()) { rollback(db_); return st; }
    return mid;
}

Result<std::size_t> Database::batch_create_messages(
    std::string_view conv_id, const std::vector<NewMessage>& messages, std::size_t batch_size) {
    std::lock_guard lock(mutex_);
    if (!db_) return Status{StatusCode::unavailable,"database not open"};
    if (batch_size==0) return Status{StatusCode::invalid_argument,"batch_size must be > 0"};
    std::size_t total=0;
    for (std::size_t start=0; start<messages.size(); start+=batch_size) {
        auto st=begin(db_); if(!st.ok()) return st;
        Statement s(db_, R"SQL(INSERT INTO messages
(id,conv_id,parent_id,role,text,model,status,version_group_id,version_num,weight,attachments,metadata,created,semantic_status)
VALUES(?,?,?,?,?,?,'active',?,1,?,?,?,?,'pending'))SQL");
        if(!s.ok()){rollback(db_); return sqlite_status(db_,"prepare batch insert",s.rc());}
        const auto end=std::min(messages.size(),start+batch_size);
        for(std::size_t idx=start;idx<end;++idx){
            const auto& m=messages[idx]; if(m.text.empty()) continue;
            sqlite3_reset(s.get()); sqlite3_clear_bindings(s.get());
            const std::string mid=make_id("m_"); const std::string vg=make_id("vg_"); const std::string now=now_iso8601_utc();
            int i=1;
            Status bst=bind_text(db_,s.get(),i++,mid); if(bst.ok()) bst=bind_text(db_,s.get(),i++,conv_id);
            if(bst.ok()) bst=bind_optional_text(db_,s.get(),i++,m.parent_id);
            if(bst.ok()) bst=bind_text(db_,s.get(),i++,m.role);
            if(bst.ok()) bst=bind_text(db_,s.get(),i++,m.text);
            if(bst.ok()) bst=bind_optional_text(db_,s.get(),i++,m.model);
            if(bst.ok()) bst=bind_text(db_,s.get(),i++,vg);
            sqlite3_bind_double(s.get(),i++,m.weight);
            if(bst.ok()) bst=bind_text(db_,s.get(),i++,m.attachments_json);
            if(bst.ok()) bst=bind_text(db_,s.get(),i++,m.metadata_json);
            if(bst.ok()) bst=bind_text(db_,s.get(),i++,now);
            if(!bst.ok()){rollback(db_); return bst;}
            if(sqlite3_step(s.get())!=SQLITE_DONE){auto e=sqlite_status(db_,"batch insert");rollback(db_);return e;}
            ++total;
        }
        if(auto x=touch_conversation_unlocked(conv_id);!x.ok()){rollback(db_);return x;}
        st=commit(db_); if(!st.ok()){rollback(db_);return st;}
    }
    return total;
}

Result<std::optional<Message>> Database::get_message(std::string_view id) const {
    std::lock_guard lock(mutex_);
    if(!db_) return Status{StatusCode::unavailable,"database not open"};
    Statement s(db_,"SELECT id,conv_id,parent_id,role,text,model,status,version_group_id,version_num,weight,attachments,metadata,created,semantic_status FROM messages WHERE id=?");
    if(!s.ok()) return sqlite_status(db_,"prepare get message",s.rc());
    if(auto st=bind_text(db_,s.get(),1,id);!st.ok()) return st;
    int rc=sqlite3_step(s.get()); if(rc==SQLITE_DONE)return std::optional<Message>{}; if(rc!=SQLITE_ROW)return sqlite_status(db_,"get message",rc);
    return std::optional<Message>{read_message(s.get())};
}

Result<std::vector<Message>> Database::get_messages(std::string_view conv_id,bool include_all) const {
    std::lock_guard lock(mutex_);
    if(!db_) return Status{StatusCode::unavailable,"database not open"};
    const char* sql=include_all?
      "SELECT id,conv_id,parent_id,role,text,model,status,version_group_id,version_num,weight,attachments,metadata,created,semantic_status FROM messages WHERE conv_id=? ORDER BY created":
      "SELECT id,conv_id,parent_id,role,text,model,status,version_group_id,version_num,weight,attachments,metadata,created,semantic_status FROM messages WHERE conv_id=? AND status='active' ORDER BY created";
    Statement s(db_,sql); if(!s.ok()) return sqlite_status(db_,"prepare get messages",s.rc());
    if(auto st=bind_text(db_,s.get(),1,conv_id);!st.ok()) return st;
    std::vector<Message> out; for(;;){int rc=sqlite3_step(s.get()); if(rc==SQLITE_DONE)break; if(rc!=SQLITE_ROW)return sqlite_status(db_,"get messages",rc); out.push_back(read_message(s.get()));}
    return out;
}

Result<std::vector<Message>> Database::get_versions(std::string_view vg) const {
    std::lock_guard lock(mutex_);
    if(!db_) return Status{StatusCode::unavailable,"database not open"};
    Statement s(db_,"SELECT id,conv_id,parent_id,role,text,model,status,version_group_id,version_num,weight,attachments,metadata,created,semantic_status FROM messages WHERE version_group_id=? ORDER BY version_num");
    if (!s.ok()) return sqlite_status(db_, "prepare versions", s.rc());
    if (auto st = bind_text(db_, s.get(), 1, vg); !st.ok()) return st;
    std::vector<Message> out;
    for (;;) {
        const int rc = sqlite3_step(s.get());
        if (rc == SQLITE_DONE) break;
        if (rc != SQLITE_ROW) return sqlite_status(db_, "versions", rc);
        out.push_back(read_message(s.get()));
    }
    return out;
}

Result<std::string> Database::edit_message(std::string_view id,std::string_view new_text){
    auto old=get_message(id); if(!old.ok())return old.status(); if(!old.value())return Status{StatusCode::not_found,"message not found"};
    auto& m=*old.value(); return create_message(m.conv_id,new_text,m.role,m.model,m.parent_id,m.attachments_json,m.version_group_id,m.weight,m.metadata_json);
}

Status Database::restore_version(std::string_view id){
    std::lock_guard lock(mutex_); if(!db_)return{StatusCode::unavailable,"database not open"};
    Statement q(db_,"SELECT version_group_id FROM messages WHERE id=?"); if(!q.ok())return sqlite_status(db_,"prepare restore",q.rc()); if(auto st=bind_text(db_,q.get(),1,id);!st.ok())return st;
    if (sqlite3_step(q.get()) != SQLITE_ROW) {
        return {StatusCode::not_found, "message not found"};
    }
    auto vg = col_optional_text(q.get(), 0);
    if (!vg) return {StatusCode::invalid_argument, "message has no version group"};
    auto st=begin(db_);if(!st.ok())return st;
    Statement old(db_,"UPDATE messages SET status='version' WHERE version_group_id=? AND status='active'"); if(!old.ok()){rollback(db_);return sqlite_status(db_,"prepare restore old",old.rc());} bind_text(db_,old.get(),1,*vg); if(sqlite3_step(old.get())!=SQLITE_DONE){auto e=sqlite_status(db_,"restore old");rollback(db_);return e;}
    Statement cur(db_,"UPDATE messages SET status='active' WHERE id=?");if(!cur.ok()){rollback(db_);return sqlite_status(db_,"prepare restore current",cur.rc());} bind_text(db_,cur.get(),1,id);if(sqlite3_step(cur.get())!=SQLITE_DONE){auto e=sqlite_status(db_,"restore current");rollback(db_);return e;}
    st=commit(db_);if(!st.ok())rollback(db_);return st;
}

Status Database::set_message_status(std::string_view id,std::string_view status){
    if(status!="active"&&status!="excluded"&&status!="version"&&status!="deleted")return{StatusCode::invalid_argument,"invalid message status"};
    std::lock_guard lock(mutex_);if(!db_)return{StatusCode::unavailable,"database not open"};Statement s(db_,"UPDATE messages SET status=? WHERE id=?");if(!s.ok())return sqlite_status(db_,"prepare set status",s.rc());if(auto st=bind_text(db_,s.get(),1,status);!st.ok())return st;if(auto st=bind_text(db_,s.get(),2,id);!st.ok())return st;return sqlite3_step(s.get())==SQLITE_DONE?Status::Ok():sqlite_status(db_,"set status");
}

Result<std::vector<Message>> Database::get_unanalysed_messages(int limit) const{
    std::lock_guard lock(mutex_);if(!db_)return Status{StatusCode::unavailable,"database not open"};if(limit<=0)return Status{StatusCode::invalid_argument,"limit must be > 0"};
    Statement s(db_,"SELECT id,conv_id,parent_id,role,text,model,status,version_group_id,version_num,weight,attachments,metadata,created,semantic_status FROM messages WHERE semantic_status='pending' AND status='active' AND length(text)>=20 ORDER BY created ASC LIMIT ?");if(!s.ok())return sqlite_status(db_,"prepare pending",s.rc());sqlite3_bind_int(s.get(),1,limit);std::vector<Message> out;for(;;){int rc=sqlite3_step(s.get());if(rc==SQLITE_DONE)break;if(rc!=SQLITE_ROW)return sqlite_status(db_,"pending",rc);out.push_back(read_message(s.get()));}return out;
}

Result<std::size_t> Database::count_pending_semantic() const{
    std::lock_guard lock(mutex_);if(!db_)return Status{StatusCode::unavailable,"database not open"};Statement s(db_,"SELECT COUNT(*) FROM messages WHERE semantic_status='pending'");if(!s.ok())return sqlite_status(db_,"prepare pending count",s.rc());if(sqlite3_step(s.get())!=SQLITE_ROW)return sqlite_status(db_,"pending count");return static_cast<std::size_t>(sqlite3_column_int64(s.get(),0));
}

Status Database::mark_analysed(std::string_view id,std::string_view metadata_json){
    std::lock_guard lock(mutex_);if(!db_)return{StatusCode::unavailable,"database not open"};Statement s(db_,"UPDATE messages SET metadata=?,semantic_status='done' WHERE id=?");if(!s.ok())return sqlite_status(db_,"prepare mark analysed",s.rc());if(auto st=bind_text(db_,s.get(),1,metadata_json);!st.ok())return st;if(auto st=bind_text(db_,s.get(),2,id);!st.ok())return st;return sqlite3_step(s.get())==SQLITE_DONE?Status::Ok():sqlite_status(db_,"mark analysed");
}

}  // namespace loom
