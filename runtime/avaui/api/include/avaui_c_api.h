#ifndef AVAUI_C_API_H
#define AVAUI_C_API_H

#include "avalang.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
  #define AVAUI_API __declspec(dllexport)
#else
  #define AVAUI_API __attribute__((visibility("default")))
#endif

typedef struct AvaComponent AvaComponent;
typedef struct AvaComponentTree AvaComponentTree;

AVAUI_API AvaComponentTree* ava_ui_create_tree(void);
AVAUI_API void              ava_ui_destroy_tree(AvaComponentTree* tree);

AVAUI_API AvaComponent* ava_ui_create_component(const char* type);
AVAUI_API void           ava_ui_destroy_component(AvaComponent* component);

AVAUI_API void ava_ui_set_property(AvaVM* vm, AvaComponent* comp, const char* key, ava_value_t value);
AVAUI_API int  ava_ui_has_property(AvaComponent* comp, const char* key);
AVAUI_API ava_value_t ava_ui_get_property(AvaVM* vm, AvaComponent* comp, const char* key);
AVAUI_API void ava_ui_remove_property(AvaComponent* comp, const char* key);

AVAUI_API size_t ava_ui_property_count(AvaComponent* comp);
AVAUI_API const char* ava_ui_property_key_at(AvaComponent* comp, size_t index);

AVAUI_API void ava_ui_add_child(AvaComponent* parent, AvaComponent* child);
AVAUI_API void ava_ui_remove_child(AvaComponent* parent, AvaComponent* child);
AVAUI_API size_t ava_ui_child_count(AvaComponent* parent);
AVAUI_API AvaComponent* ava_ui_get_child(AvaComponent* parent, size_t index);

AVAUI_API void ava_ui_set_event(AvaVM* vm, AvaComponent* comp, const char* event, ava_value_t callback);
AVAUI_API int  ava_ui_has_event(AvaComponent* comp, const char* event);
AVAUI_API ava_value_t ava_ui_get_event(AvaVM* vm, AvaComponent* comp, const char* event);

AVAUI_API size_t ava_ui_event_count(AvaComponent* comp);
AVAUI_API const char* ava_ui_event_key_at(AvaComponent* comp, size_t index);

AVAUI_API void ava_ui_set_id(AvaComponent* comp, const char* id);
AVAUI_API const char* ava_ui_get_id(AvaComponent* comp);

AVAUI_API void ava_ui_set_layout(AvaComponent* comp, int layout);
AVAUI_API int  ava_ui_get_layout(AvaComponent* comp);

AVAUI_API void ava_ui_set_root(AvaComponentTree* tree, AvaComponent* root);
AVAUI_API AvaComponent* ava_ui_get_root(AvaComponentTree* tree);

AVAUI_API const char* ava_ui_get_component_type(AvaComponent* comp);
AVAUI_API const char* ava_ui_tree_to_json(AvaComponentTree* tree);
AVAUI_API void ava_ui_json_free(char* json);

AVAUI_API AvaComponentTree* ava_ui_parse_avaui_text(
    const char* text,
    char** out_state_json,
    char** out_imports_json,
    char** out_methods_text,
    char** out_error,
    char** out_extends,
    char** out_routes_json
);

AVAUI_API char* ava_ui_write_avaui_text(
    AvaComponentTree* tree,
    const char* state_json,
    const char* imports_json,
    const char* methods_text,
    const char* extends,
    const char* routes_json
);

AVAUI_API void ava_ui_text_free(char* text);

#ifdef __cplusplus
}
#endif

#endif
