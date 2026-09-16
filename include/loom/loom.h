#ifndef LOOM_H
#define LOOM_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LoomContext LoomContext;

LoomContext* loom_init(const char* data_dir);
void         loom_shutdown(LoomContext* ctx);

const char* loom_list_conversations(LoomContext* ctx, int limit);
const char* loom_create_conversation(LoomContext* ctx, const char* title);
int         loom_delete_conversation(LoomContext* ctx, const char* conv_id);
const char* loom_get_messages(LoomContext* ctx, const char* conv_id);

typedef void (*LoomStreamCallback)(const char* chunk, int done, void* user_data);
void loom_chat(LoomContext* ctx, const char* conv_id, const char* user_message,
               const char* model_id, int context_depth,
               LoomStreamCallback callback, void* user_data);

const char* loom_get_nodes(LoomContext* ctx, const char* filter_json);
const char* loom_get_edges(LoomContext* ctx, const char* filter_json);
const char* loom_expand_graph(LoomContext* ctx, const char* seed_ids_json, int depth);

typedef void (*LoomProgressCallback)(int current, int total, const char* status, void* ud);
const char* loom_import_file(LoomContext* ctx, const char* path, const char* title,
                             LoomProgressCallback cb, void* ud);
const char* loom_export_conversation(LoomContext* ctx, const char* conv_id, const char* fmt);

const char* loom_select_context(LoomContext* ctx, const char* text,
                                int depth, int max_tokens);

const char* loom_semantic_status(LoomContext* ctx);
void        loom_semantic_pause(LoomContext* ctx);
void        loom_semantic_resume(LoomContext* ctx);
void        loom_semantic_wake(LoomContext* ctx);

const char* loom_list_memory(LoomContext* ctx);
const char* loom_create_memory(LoomContext* ctx, const char* json);
const char* loom_update_memory(LoomContext* ctx, const char* id, const char* json);
int         loom_delete_memory(LoomContext* ctx, const char* id);

const char* loom_get_config(LoomContext* ctx);
void        loom_set_config(LoomContext* ctx, const char* key, const char* value);
const char* loom_get_models(LoomContext* ctx);

/* Every const char* returned by Loom is heap allocated and must be released here. */
void loom_free_string(const char* str);

#ifdef __cplusplus
}
#endif
#endif
