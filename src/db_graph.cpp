#include "loom/db.h"
#include "db_internal.h"

namespace loom {
using namespace db_internal;

Result<std::string> Database::create_node(std::string_view label,std::string_view kind,std::string_view content,std::string tags_json,std::string metadata_json,std::optional<std::string> node_id){
    std::lock_guard lock(mutex_);if(!db_)return Status{StatusCode::unavailable,"database not open"};const std::string id=node_id.value_or(make_id("n_"));const auto now=now_iso8601_utc();Statement s(db_,"INSERT OR IGNORE INTO nodes(id,kind,label,content,tags,metadata,created) VALUES(?,?,?,?,?,?,?)");if(!s.ok())return sqlite_status(db_,"prepare create node",s.rc());const std::string_view vals[]={id,kind,label,content,tags_json,metadata_json,now};for(int i=0;i<7;++i)if(auto st=bind_text(db_,s.get(),i+1,vals[i]);!st.ok())return st;if(sqlite3_step(s.get())!=SQLITE_DONE)return sqlite_status(db_,"create node");return id;
}

Result<std::optional<Node>> Database::get_node(std::string_view id) const{
    std::lock_guard lock(mutex_);if(!db_)return Status{StatusCode::unavailable,"database not open"};Statement s(db_,"SELECT id,kind,label,content,tags,metadata,created FROM nodes WHERE id=?");if(!s.ok())return sqlite_status(db_,"prepare get node",s.rc());if(auto st=bind_text(db_,s.get(),1,id);!st.ok())return st;int rc=sqlite3_step(s.get());if(rc==SQLITE_DONE)return std::optional<Node>{};if(rc!=SQLITE_ROW)return sqlite_status(db_,"get node",rc);return std::optional<Node>{read_node(s.get())};
}

Result<std::optional<Node>> Database::find_node(std::string_view label,std::optional<std::string> kind) const{
    std::lock_guard lock(mutex_);if(!db_)return Status{StatusCode::unavailable,"database not open"};Statement s(db_,kind?"SELECT id,kind,label,content,tags,metadata,created FROM nodes WHERE label=? AND kind=? LIMIT 1":"SELECT id,kind,label,content,tags,metadata,created FROM nodes WHERE label=? LIMIT 1");if(!s.ok())return sqlite_status(db_,"prepare find node",s.rc());if(auto st=bind_text(db_,s.get(),1,label);!st.ok())return st;if(kind)if(auto st=bind_text(db_,s.get(),2,*kind);!st.ok())return st;int rc=sqlite3_step(s.get());if(rc==SQLITE_DONE)return std::optional<Node>{};if(rc!=SQLITE_ROW)return sqlite_status(db_,"find node",rc);return std::optional<Node>{read_node(s.get())};
}

Result<std::string> Database::get_or_create_node(std::string_view label,std::string_view kind){
    auto found=find_node(label,std::string(kind));if(!found.ok())return found.status();if(found.value())return found.value()->id;return create_node(label,kind);
}

Result<std::vector<Node>> Database::list_nodes(std::optional<std::string> kind,int limit) const{
    std::lock_guard lock(mutex_);if(!db_)return Status{StatusCode::unavailable,"database not open"};if(limit<=0)return Status{StatusCode::invalid_argument,"limit must be > 0"};Statement s(db_,kind?"SELECT id,kind,label,content,tags,metadata,created FROM nodes WHERE kind=? ORDER BY created DESC LIMIT ?":"SELECT id,kind,label,content,tags,metadata,created FROM nodes ORDER BY created DESC LIMIT ?");if(!s.ok())return sqlite_status(db_,"prepare list nodes",s.rc());int i=1;if(kind){if(auto st=bind_text(db_,s.get(),i++,*kind);!st.ok())return st;}sqlite3_bind_int(s.get(),i,limit);std::vector<Node> out;for(;;){int rc=sqlite3_step(s.get());if(rc==SQLITE_DONE)break;if(rc!=SQLITE_ROW)return sqlite_status(db_,"list nodes",rc);out.push_back(read_node(s.get()));}return out;
}

Status Database::delete_node(std::string_view id) {
    std::lock_guard lock(mutex_);
    if (!db_) return {StatusCode::unavailable, "database not open"};
    auto st = begin(db_);
    if (!st.ok()) return st;

    Statement links(db_, "DELETE FROM links WHERE src=? OR dst=?");
    if (!links.ok()) { rollback(db_); return sqlite_status(db_, "prepare delete node links", links.rc()); }
    if (auto x = bind_text(db_, links.get(), 1, id); !x.ok()) { rollback(db_); return x; }
    if (auto x = bind_text(db_, links.get(), 2, id); !x.ok()) { rollback(db_); return x; }
    if (sqlite3_step(links.get()) != SQLITE_DONE) { auto e = sqlite_status(db_, "delete node links"); rollback(db_); return e; }

    Statement node(db_, "DELETE FROM nodes WHERE id=?");
    if (!node.ok()) { rollback(db_); return sqlite_status(db_, "prepare delete node", node.rc()); }
    if (auto x = bind_text(db_, node.get(), 1, id); !x.ok()) { rollback(db_); return x; }
    if (sqlite3_step(node.get()) != SQLITE_DONE) { auto e = sqlite_status(db_, "delete node"); rollback(db_); return e; }

    st = commit(db_);
    if (!st.ok()) rollback(db_);
    return st;
}

Result<std::string> Database::create_link(std::string_view src,std::string_view dst,std::string_view link_type,double weight,std::string metadata_json){
    std::lock_guard lock(mutex_);if(!db_)return Status{StatusCode::unavailable,"database not open"};Statement q(db_,"SELECT id FROM links WHERE src=? AND dst=? AND link_type=? LIMIT 1");if(!q.ok())return sqlite_status(db_,"prepare link lookup",q.rc());bind_text(db_,q.get(),1,src);bind_text(db_,q.get(),2,dst);bind_text(db_,q.get(),3,link_type);if(sqlite3_step(q.get())==SQLITE_ROW){const auto id=col_text(q.get(),0);Statement u(db_,"UPDATE links SET weight=?,metadata=? WHERE id=?");if(!u.ok())return sqlite_status(db_,"prepare link update",u.rc());sqlite3_bind_double(u.get(),1,weight);bind_text(db_,u.get(),2,metadata_json);bind_text(db_,u.get(),3,id);if(sqlite3_step(u.get())!=SQLITE_DONE)return sqlite_status(db_,"update link");return id;}
    const auto id=make_id("l_");const auto now=now_iso8601_utc();Statement s(db_,"INSERT INTO links(id,src,dst,link_type,weight,metadata,created) VALUES(?,?,?,?,?,?,?)");if(!s.ok())return sqlite_status(db_,"prepare create link",s.rc());bind_text(db_,s.get(),1,id);bind_text(db_,s.get(),2,src);bind_text(db_,s.get(),3,dst);bind_text(db_,s.get(),4,link_type);sqlite3_bind_double(s.get(),5,weight);bind_text(db_,s.get(),6,metadata_json);bind_text(db_,s.get(),7,now);if(sqlite3_step(s.get())!=SQLITE_DONE)return sqlite_status(db_,"create link");return id;
}

Result<std::vector<Link>> Database::get_links(std::optional<std::string> node_id,std::optional<std::string> link_type) const{
    std::lock_guard lock(mutex_);if(!db_)return Status{StatusCode::unavailable,"database not open"};std::string sql="SELECT id,src,dst,link_type,weight,metadata,created FROM links";if(node_id&&link_type)sql+=" WHERE (src=? OR dst=?) AND link_type=?";else if(node_id)sql+=" WHERE src=? OR dst=?";else if(link_type)sql+=" WHERE link_type=?";Statement s(db_,sql);if(!s.ok())return sqlite_status(db_,"prepare links",s.rc());int i=1;if(node_id){bind_text(db_,s.get(),i++,*node_id);bind_text(db_,s.get(),i++,*node_id);}if(link_type)bind_text(db_,s.get(),i++,*link_type);std::vector<Link> out;for(;;){int rc=sqlite3_step(s.get());if(rc==SQLITE_DONE)break;if(rc!=SQLITE_ROW)return sqlite_status(db_,"links",rc);out.push_back(read_link(s.get()));}return out;
}

Status Database::delete_link(std::string_view id){
    std::lock_guard lock(mutex_);if(!db_)return{StatusCode::unavailable,"database not open"};Statement s(db_,"DELETE FROM links WHERE id=?");if(!s.ok())return sqlite_status(db_,"prepare delete link",s.rc());if(auto st=bind_text(db_,s.get(),1,id);!st.ok())return st;return sqlite3_step(s.get())==SQLITE_DONE?Status::Ok():sqlite_status(db_,"delete link");
}

}  // namespace loom
