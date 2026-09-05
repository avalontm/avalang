#ifndef AVA_COMPILER_OBFUSCATE_H
#define AVA_COMPILER_OBFUSCATE_H

#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"
#include "../vm/proto.h"

namespace ava {


struct ObfuscateOptions {
    uint64_t module_seed = 0;
    bool strip_debug_lines = true;
    bool obfuscate_strings = false;
    bool flatten_control_flow = false;
    avastd::vector<avastd::string> flatten_functions;
};

struct SymbolMapEntry {
    avastd::string kind;       
    avastd::string original;    
    avastd::string obfuscated;  
};

void ObfuscateProto(Proto& root,
                     const ObfuscateOptions& options,
                     avastd::vector<SymbolMapEntry>* out_symbol_map = nullptr);

void DeobfuscateStrings(Proto& root, uint64_t module_seed);

bool FlattenProtoControlFlow(Proto& proto, uint64_t seed, const avastd::string& tag);

avastd::string FormatSymbolMap(const avastd::vector<SymbolMapEntry>& map);

} // namespace ava

#endif // AVA_COMPILER_OBFUSCATE_H
