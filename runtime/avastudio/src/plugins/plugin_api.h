#ifndef AVA_STUDIO_PLUGIN_API_H
#define AVA_STUDIO_PLUGIN_API_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AVA_STUDIO_PLUGIN_ABI_VERSION 7

typedef struct AvaStudioHost AvaStudioHost;

typedef struct AvaPanelContext AvaPanelContext;

typedef enum AvaDockSlot {
    AVA_DOCK_LEFT = 0,
    AVA_DOCK_RIGHT = 1,
    AVA_DOCK_BOTTOM = 2,
    AVA_DOCK_CENTER = 3,
} AvaDockSlot;

typedef void (*AvaPanelDrawFn)(AvaPanelContext* ctx, void* user_data);

typedef struct AvaUiApi {
    void (*label)(AvaPanelContext* ctx, const char* text);

    void (*text_wrapped)(AvaPanelContext* ctx, const char* text);

    bool (*button)(AvaPanelContext* ctx, const char* label);

    bool (*input_text)(AvaPanelContext* ctx, const char* label, char* buffer, size_t buffer_size);

    bool (*input_text_multiline)(AvaPanelContext* ctx, const char* label, char* buffer, size_t buffer_size,
                                  float height);

    bool (*combo)(AvaPanelContext* ctx, const char* label, int* current_index, const char* const* items,
                  int items_count);

    void (*separator)(AvaPanelContext* ctx);

    void (*same_line)(AvaPanelContext* ctx);
    void (*spacing)(AvaPanelContext* ctx);

    bool (*begin_child)(AvaPanelContext* ctx, const char* id, float height);
    void (*end_child)(AvaPanelContext* ctx);

    void (*scroll_to_bottom)(AvaPanelContext* ctx);

    bool (*input_text_multiline_submit)(AvaPanelContext* ctx, const char* label, char* buffer, size_t buffer_size,
                                         float height, bool* out_submit);

    bool (*input_text_multiline_submit_hint)(AvaPanelContext* ctx, const char* label, const char* hint,
                                              char* buffer, size_t buffer_size, float height, bool* out_submit);

    bool (*button_disabled)(AvaPanelContext* ctx, const char* label, bool disabled);

    float (*text_line_height)(AvaPanelContext* ctx);

    void (*text_colored)(AvaPanelContext* ctx, const char* text, float r, float g, float b, float a);

    void (*selectable_message)(AvaPanelContext* ctx, const char* id, const char* text,
                                float text_r, float text_g, float text_b, float text_a,
                                float bg_r, float bg_g, float bg_b, float bg_a);
} AvaUiApi;

typedef struct AvaHostServices {

    const char* (*get_project_root)(AvaStudioHost* host);

    bool (*get_active_file)(AvaStudioHost* host, const char** out_path, const char** out_content,
                             int* out_selection_start, int* out_selection_end);

    bool (*get_last_run_output)(AvaStudioHost* host, const char** out_text, bool* out_had_error);

    void (*log)(AvaStudioHost* host, const char* message);

    bool (*apply_edit)(AvaStudioHost* host, const char* path, const char* new_content, const char* description,
                        const char** out_error);

    bool (*run_project)(AvaStudioHost* host, const char** out_output, bool* out_had_error, const char** out_error);

    bool (*design_add_component)(AvaStudioHost* host, const char* parent_id, const char* type, const char* id,
                                  const char* properties_kv, const char** out_error);

    bool (*design_edit_component)(AvaStudioHost* host, const char* node_id, const char* properties_kv,
                                   const char* new_id, const char** out_error);
} AvaHostServices;

typedef struct AvaPanelRegistration {

    const char* name;
    AvaPanelDrawFn draw;
    void* user_data;
    AvaDockSlot default_dock_slot;

    bool is_settings;
} AvaPanelRegistration;

struct AvaStudioHost {

    int abi_version;

    void* _internal_host_reserved;

    AvaUiApi ui;
    AvaHostServices services;

    int (*register_panel)(AvaStudioHost* host, const AvaPanelRegistration* registration);
};

typedef int (*AvaPluginAbiVersionFn)(void);
typedef bool (*AvaPluginInitFn)(AvaStudioHost* host);
typedef void (*AvaPluginShutdownFn)(void);

#define AVA_PLUGIN_ABI_VERSION_SYMBOL "ava_plugin_abi_version"
#define AVA_PLUGIN_INIT_SYMBOL "ava_plugin_init"
#define AVA_PLUGIN_SHUTDOWN_SYMBOL "ava_plugin_shutdown"

typedef const char* (*AvaPluginMetadataFn)(void);

#define AVA_PLUGIN_DISPLAY_NAME_SYMBOL "ava_plugin_display_name"
#define AVA_PLUGIN_VERSION_SYMBOL "ava_plugin_version"
#define AVA_PLUGIN_AUTHOR_SYMBOL "ava_plugin_author"

#ifdef __cplusplus
}
#endif

#endif
