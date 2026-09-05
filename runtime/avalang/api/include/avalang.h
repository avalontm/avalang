#ifndef AVALANG_H
#define AVALANG_H

/*
 * AvaLang public C API.
 */

#if defined(AVA_BAREKERNEL_TARGET_BINDING) && defined(CKM_CAP_LIBSTDCPP) && !CKM_CAP_LIBSTDCPP
typedef __UINT8_TYPE__  uint8_t;
typedef __UINT64_TYPE__ uint64_t;
typedef __INT64_TYPE__  int64_t;
typedef __SIZE_TYPE__   size_t;
#ifndef NULL
  #ifdef __cplusplus
    #define NULL 0
  #else
    #define NULL ((void*)0)
  #endif
#endif
#else
#include <stdint.h>
#include <stddef.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
  #define AVA_API __declspec(dllexport)
#else
  #define AVA_API __attribute__((visibility("default")))
#endif

/* ---------------------------------------------------------------------
 * Opaque handles
 * ------------------------------------------------------------------- */

typedef struct AvaVM        AvaVM;        /* one VM instance / isolate   */
typedef struct AvaModule    AvaModule;    /* compiled bytecode module    */
typedef struct AvaCoroutine AvaCoroutine; /* suspendable execution state */

/* ---------------------------------------------------------------------
 * Value type: tagged union passed by value across the ABI.
 * Strings/Lists/Dicts/Functions/Instances are represented by a handle
 * (ref-counted internally); the C API never exposes their raw layout.
 * ------------------------------------------------------------------- */

typedef enum AvaValueType {
    AVA_NIL = 0,
    AVA_BOOL,
    AVA_NUMBER,
    AVA_STRING,
    AVA_LIST,
    AVA_DICT,
    AVA_FUNCTION,
    AVA_INSTANCE,
    AVA_CLASS,
    AVA_COROUTINE,
    AVA_NATIVE,
    AVA_BOUND,
    AVA_EXCEPTION,
    AVA_MODULE
} AvaValueType;

typedef struct AvaRef { uint64_t id; } AvaRef;

typedef struct ava_value_t {
    AvaValueType type;
    union {
        int      b;      /* AVA_BOOL     */
        double   n;       /* AVA_NUMBER   */
        AvaRef   ref;      /* everything ref-counted (string/list/dict/...) */
        AvaRef   err;     /* AVA_EXCEPTION */
    } as;
} ava_value_t;

typedef struct {
    const char* key;
    size_t key_len;
    ava_value_t value;
} ava_dict_pair_t;

/* ---------------------------------------------------------------------
 * Native function callback: how host languages expose functions to
 * scripts. Called by the VM with args already marshalled as ava_value_t.
 * ------------------------------------------------------------------- */

typedef ava_value_t (*AvaNativeFn)(
    AvaVM* vm,
    const ava_value_t* args,
    size_t arg_count,
    void* user_data
);

/* ---------------------------------------------------------------------
 * VM lifecycle
 * ------------------------------------------------------------------- */

AVA_API AvaVM* ava_vm_create(void);
AVA_API void   ava_vm_destroy(AvaVM* vm);

AVA_API void ava_vm_set_current_dir(AvaVM* vm, const char* dir);

AVA_API void ava_vm_add_search_path(AvaVM* vm, const char* path);
AVA_API void ava_vm_set_stdlib_path(AvaVM* vm, const char* path);

AVA_API char* ava_vm_get_stdlib_path(AvaVM* vm);

AVA_API void ava_vm_collect_garbage(AvaVM* vm, int64_t* out_collected);

AVA_API void ava_vm_register_native(
    AvaVM* vm,
    const char* name,
    AvaNativeFn fn,
    void* user_data
);

typedef void (*AvaPrintFn)(const char* utf8, size_t len, void* user_data);

AVA_API void ava_vm_set_print_callback(
    AvaVM* vm,
    AvaPrintFn fn,
    void* user_data
);

typedef char* (*AvaInputFn)(const char* prompt_utf8, size_t prompt_len, void* user_data);

AVA_API void ava_vm_set_input_callback(
    AvaVM* vm,
    AvaInputFn fn,
    void* user_data
);

typedef void (*AvaAlertFn)(const char* utf8, size_t len, void* user_data);

AVA_API void ava_vm_set_alert_callback(
    AvaVM* vm,
    AvaAlertFn fn,
    void* user_data
);

typedef void (*AvaNavigateFn)(const char* utf8, size_t len, void* user_data);

AVA_API void ava_vm_set_navigate_callback(
    AvaVM* vm,
    AvaNavigateFn fn,
    void* user_data
);

/* ---------------------------------------------------------------------
 * Compilation
 * ------------------------------------------------------------------- */

AVA_API AvaModule* ava_compile(
    AvaVM* vm,
    const char* source,
    const char* source_name,
    char** out_error
);

AVA_API void ava_module_destroy(AvaModule* module);

typedef struct AvaModuleSerializeOptions {
    int strip_debug_info;     
    int obfuscate;             
    uint64_t obfuscate_seed;   
    int obfuscate_strings;   
    int flatten_control_flow;  
} AvaModuleSerializeOptions;

AVA_API uint8_t* ava_module_serialize(
    AvaModule* module,
    const AvaModuleSerializeOptions* options,
    size_t* out_len,
    char** out_symbol_map
);

AVA_API AvaModule* ava_module_deserialize(
    AvaVM* vm,
    const uint8_t* bytes,
    size_t len,
    char** out_error
);


AVA_API void ava_module_deobfuscate_strings(AvaModule* module, uint64_t seed);

/* ---------------------------------------------------------------------
 * Execution
 * ------------------------------------------------------------------- */

AVA_API void ava_run(AvaVM* vm, AvaModule* module, ava_value_t* out_result, char** out_error);

AVA_API void ava_call(
    AvaVM* vm,
    ava_value_t callable,
    const ava_value_t* args,
    size_t arg_count,
    ava_value_t* out_result,
    char** out_error
);

AVA_API ava_value_t ava_get_global(AvaVM* vm, const char* name);
AVA_API void ava_set_global(AvaVM* vm, const char* name, ava_value_t value);

AVA_API ava_value_t ava_import(AvaVM* vm, const char* module_path, const char* alias, char** out_error);

/* ---------------------------------------------------------------------
 * Coroutines
 * ------------------------------------------------------------------- */

AVA_API AvaCoroutine* ava_coroutine_create(AvaVM* vm, ava_value_t function);
AVA_API void          ava_coroutine_destroy(AvaCoroutine* co);

typedef enum AvaCoStatus { AVA_CO_SUSPENDED, AVA_CO_RUNNING, AVA_CO_DEAD } AvaCoStatus;

AVA_API AvaCoStatus ava_coroutine_resume(
    AvaVM* vm,
    AvaCoroutine* co,
    const ava_value_t* args,
    size_t arg_count,
    ava_value_t* out_values,
    size_t out_capacity,
    size_t* out_count
);

AVA_API AvaCoStatus ava_coroutine_status(AvaVM* vm, AvaCoroutine* co);

/* ---------------------------------------------------------------------
 * Value helpers (construction / inspection of ref-counted values)
 * ------------------------------------------------------------------- */

AVA_API ava_value_t ava_string_create(AvaVM* vm, const char* utf8, size_t len);
AVA_API const char* ava_string_data(AvaVM* vm, ava_value_t str, size_t* out_len);

AVA_API ava_value_t ava_list_create(AvaVM* vm);
AVA_API void         ava_list_append(AvaVM* vm, ava_value_t list, ava_value_t item);
AVA_API size_t       ava_list_length(AvaVM* vm, ava_value_t list);
AVA_API ava_value_t  ava_list_get(AvaVM* vm, ava_value_t list, size_t index);
AVA_API void         ava_list_insert(AvaVM* vm, ava_value_t list, size_t index, ava_value_t item);
AVA_API void         ava_list_remove(AvaVM* vm, ava_value_t list, size_t index);
AVA_API void         ava_list_set(AvaVM* vm, ava_value_t list, size_t index, ava_value_t value);

AVA_API ava_value_t ava_dict_create(AvaVM* vm);
AVA_API void         ava_dict_set(AvaVM* vm, ava_value_t dict, const char* key, ava_value_t value);
AVA_API ava_value_t  ava_dict_get(AvaVM* vm, ava_value_t dict, const char* key);
AVA_API size_t       ava_dict_length(AvaVM* vm, ava_value_t dict);
AVA_API size_t       ava_dict_entries(AvaVM* vm, ava_value_t dict, void** out_entries);
AVA_API int          ava_dict_contains(AvaVM* vm, ava_value_t dict, const char* key, size_t key_len);

AVA_API void ava_value_retain(AvaVM* vm, ava_value_t value);
AVA_API void ava_value_release(AvaVM* vm, ava_value_t value);

AVA_API void ava_string_free(char* s); 

AVA_API int ava_last_error_line(AvaVM* vm);
AVA_API int ava_last_error_column(AvaVM* vm);

AVA_API char* ava_last_error_source(AvaVM* vm);

/* ---------------------------------------------------------------------
 * UI Component Tree API
 * ------------------------------------------------------------------- */

typedef struct AvaComponent AvaComponent;
typedef struct AvaComponentTree AvaComponentTree;

AVA_API AvaComponentTree* ava_ui_create_tree(void);
AVA_API void               ava_ui_destroy_tree(AvaComponentTree* tree);

AVA_API AvaComponent* ava_ui_create_component(const char* type);
AVA_API void           ava_ui_destroy_component(AvaComponent* component);

AVA_API void ava_ui_set_property(AvaComponent* comp, const char* key, ava_value_t value);
AVA_API int  ava_ui_has_property(AvaComponent* comp, const char* key);
AVA_API ava_value_t ava_ui_get_property(AvaComponent* comp, const char* key);
AVA_API void ava_ui_remove_property(AvaComponent* comp, const char* key);

AVA_API size_t ava_ui_property_count(AvaComponent* comp);
AVA_API const char* ava_ui_property_key_at(AvaComponent* comp, size_t index);

AVA_API void ava_ui_add_child(AvaComponent* parent, AvaComponent* child);
AVA_API void ava_ui_remove_child(AvaComponent* parent, AvaComponent* child);
AVA_API size_t ava_ui_child_count(AvaComponent* parent);
AVA_API AvaComponent* ava_ui_get_child(AvaComponent* parent, size_t index);

AVA_API void ava_ui_set_event(AvaComponent* comp, const char* event, ava_value_t callback);
AVA_API int  ava_ui_has_event(AvaComponent* comp, const char* event);
AVA_API ava_value_t ava_ui_get_event(AvaComponent* comp, const char* event);

AVA_API size_t ava_ui_event_count(AvaComponent* comp);
AVA_API const char* ava_ui_event_key_at(AvaComponent* comp, size_t index);

AVA_API void ava_ui_set_id(AvaComponent* comp, const char* id);
AVA_API const char* ava_ui_get_id(AvaComponent* comp);

AVA_API void ava_ui_set_layout(AvaComponent* comp, int layout);
AVA_API int  ava_ui_get_layout(AvaComponent* comp);

AVA_API void ava_ui_set_root(AvaComponentTree* tree, AvaComponent* root);
AVA_API AvaComponent* ava_ui_get_root(AvaComponentTree* tree);

AVA_API const char* ava_ui_get_component_type(AvaComponent* comp);
AVA_API const char* ava_ui_tree_to_json(AvaComponentTree* tree);
AVA_API void ava_ui_json_free(char* json);

AVA_API AvaComponentTree* ava_ui_parse_avaui_text(
    const char* text,
    char** out_state_json,
    char** out_imports_json,
    char** out_methods_text,
    char** out_error,
    char** out_extends,
    char** out_routes_json
);

AVA_API char* ava_ui_write_avaui_text(
    AvaComponentTree* tree,
    const char* state_json,
    const char* imports_json,
    const char* methods_text,
    const char* extends,
    const char* routes_json
);

AVA_API void ava_ui_text_free(char* text);

#ifdef __cplusplus
}
#endif

#endif /* AVALANG_H */