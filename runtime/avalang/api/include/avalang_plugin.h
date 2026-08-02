#ifndef AVALANG_PLUGIN_H
#define AVALANG_PLUGIN_H

#include "avalang.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AVA_PLUGIN_API AVA_API

typedef struct AvaPlugin AvaPlugin;

typedef ava_value_t (*AvaPluginFn)(
    AvaVM* vm,
    const ava_value_t* args,
    size_t arg_count,
    void* user_data
);

typedef void (*AvaPluginInit)(AvaVM* vm, void* user_data);

typedef void (*AvaPluginShutdown)(AvaVM* vm, void* user_data);

typedef struct AvaPluginDesc {
    const char* name;
    const char* version;
    AvaPluginInit init;
    AvaPluginShutdown shutdown;
    const char** function_names;
    const AvaPluginFn* functions;
    size_t function_count;
    void* user_data;
} AvaPluginDesc;

AVA_PLUGIN_API AvaPlugin* ava_plugin_load(AvaVM* vm, const char* path, char** out_error);

AVA_PLUGIN_API void ava_plugin_unload(AvaPlugin* plugin);

AVA_PLUGIN_API int ava_plugin_register(AvaVM* vm, const AvaPluginDesc* desc);

AVA_PLUGIN_API ava_value_t ava_plugin_call(AvaPlugin* plugin, const char* name,
                                            const ava_value_t* args, size_t arg_count);

#ifdef __cplusplus
}
#endif

#endif // AVALANG_PLUGIN_H