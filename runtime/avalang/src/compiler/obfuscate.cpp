#include "obfuscate.h"
#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

namespace ava {

namespace {

uint64_t HashSymbol(uint64_t seed, const avastd::string& kind, const avastd::string& value) {
    uint64_t h = 1469598103934665603ull ^ seed;
    auto mix = [&h](const avastd::string& s) {
        for (unsigned char c : s) {
            h ^= c;
            h *= 1099511628211ull;
        }
        h ^= 0xFFu;
        h *= 1099511628211ull;
    };
    mix(kind);
    mix(value);
    return h;
}

avastd::string ToOpaqueId(uint64_t hash) {
    avastd::ostringstream oss;
    oss << "sym_" << avastd::hex << avastd::setw(16) << avastd::setfill('0') << hash;
    return oss.str();
}

avastd::string XorKeystreamTransform(uint64_t seed, uint64_t string_index, const avastd::string& data) {
    avastd::string out;
    out.resize(data.size());
    uint32_t block = 0;
    size_t pos = 0;
    while (pos < data.size()) {
        avastd::ostringstream tag;
        tag << string_index << ':' << block;
        uint64_t h = HashSymbol(seed, "string_block", tag.str());
        unsigned char key_bytes[8];
        for (int i = 0; i < 8; ++i) key_bytes[i] = static_cast<unsigned char>((h >> (i * 8)) & 0xFF);
        for (int i = 0; i < 8 && pos < data.size(); ++i, ++pos) {
            out[pos] = static_cast<char>(static_cast<unsigned char>(data[pos]) ^ key_bytes[i]);
        }
        ++block;
    }
    return out;
}

void TransformStringsNode(Proto& proto, uint64_t seed, uint64_t& next_index) {
    for (auto& c : proto.constants) {
        if (c.type != ValueType::String) continue;
        auto* str_obj = static_cast<StringObj*>(c.obj);
        if (!str_obj) continue;
        str_obj->data = XorKeystreamTransform(seed, next_index, str_obj->data);
        ++next_index;
    }
    for (auto& child : proto.child_protos) {
        TransformStringsNode(*child, seed, next_index);
    }
}

struct FlatBlock {
    size_t start;
    size_t end; 
};

bool HasUnsupportedControlFlow(const Proto& proto) {
    for (const auto& instr : proto.instructions) {
        switch (instr.op) {
            case OpCode::TRY:
            case OpCode::TRY_END:
            case OpCode::CATCH:
            case OpCode::RAISE:
            case OpCode::YIELD:
            case OpCode::RESUME:
                return true;
            default:
                break;
        }
    }
    return false;
}

avastd::vector<FlatBlock> ComputeBlocks(const avastd::vector<Instr>& ins) {
    avastd::set<size_t> leaders;
    leaders.insert(0);
    for (size_t i = 0; i < ins.size(); ++i) {
        if (ins[i].op == OpCode::JMP) {
            int64_t target = static_cast<int64_t>(i) + 1 + ins[i].bx32;
            if (target >= 0 && static_cast<size_t>(target) <= ins.size()) {
                leaders.insert(static_cast<size_t>(target));
            }
            if (i + 1 < ins.size()) leaders.insert(i + 1);
        } else if (ins[i].op == OpCode::RETURN) {
            if (i + 1 < ins.size()) leaders.insert(i + 1);
        }
    }
    avastd::vector<size_t> sorted(leaders.begin(), leaders.end());
    avastd::vector<FlatBlock> blocks;
    for (size_t k = 0; k < sorted.size(); ++k) {
        size_t start = sorted[k];
        size_t end = (k + 1 < sorted.size()) ? sorted[k + 1] : ins.size();
        if (start < end) blocks.push_back(FlatBlock{start, end});
    }
    return blocks;
}

size_t FindBlockForTarget(const avastd::vector<FlatBlock>& blocks, size_t target) {
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (blocks[i].start == target) return i;
    }
    return blocks.size();
}

uint64_t NextPrng(uint64_t seed, const avastd::string& tag, uint64_t& counter) {
    return HashSymbol(seed, "cf_prng", tag + ":" + avastd::to_string(counter++));
}

} 

namespace {

void ObfuscateProtoNode(Proto& proto, const ObfuscateOptions& options,
                         avastd::vector<SymbolMapEntry>* out_symbol_map) {
    if (!proto.debug_name.empty()) {
        avastd::string obfuscated = ToOpaqueId(HashSymbol(options.module_seed, "function", proto.debug_name));
        if (out_symbol_map) {
            out_symbol_map->push_back(SymbolMapEntry{"function", proto.debug_name, obfuscated});
        }
        proto.debug_name = obfuscated;
    }

    if (!proto.source_name.empty()) {
        avastd::string obfuscated = ToOpaqueId(HashSymbol(options.module_seed, "source_file", proto.source_name));
        if (out_symbol_map) {
            out_symbol_map->push_back(SymbolMapEntry{"source_file", proto.source_name, obfuscated});
        }
        proto.source_name = obfuscated;
    }

    if (options.strip_debug_lines) {
        proto.debug_lines.clear();
    }

    for (auto& child : proto.child_protos) {
        ObfuscateProtoNode(*child, options, out_symbol_map);
    }
}

} // namespace

bool FlattenProtoControlFlow(Proto& proto, uint64_t seed, const avastd::string& tag) {
    if (proto.instructions.size() < 2) return false;
    if (HasUnsupportedControlFlow(proto)) return false;

    avastd::vector<FlatBlock> blocks = ComputeBlocks(proto.instructions);
    if (blocks.size() < 2) return false;

    const size_t exit_id = blocks.size();
    const size_t total_states = blocks.size() + 1;

    avastd::vector<uint16_t> state_const_idx(total_states);
    for (size_t s = 0; s < total_states; ++s) {
        uint64_t state_value = HashSymbol(seed, "cf_state", tag + ":" + avastd::to_string(s)) & 0xFFFFFFFFFFFFull;
        state_const_idx[s] = static_cast<uint16_t>(proto.constants.size());
        proto.constants.push_back(Value::Number(static_cast<double>(state_value)));
    }

    avastd::vector<size_t> order(total_states);
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    uint64_t shuffle_counter = 0;
    for (size_t i = order.size(); i > 1; --i) {
        uint64_t r = NextPrng(seed, tag, shuffle_counter);
        size_t j = static_cast<size_t>(r % i);
        avastd::swap(order[i - 1], order[j]);
    }

    const uint16_t state_reg = proto.num_registers;
    const uint16_t tmp_reg = static_cast<uint16_t>(proto.num_registers + 1);
    proto.num_registers = static_cast<uint16_t>(proto.num_registers + 2);

    avastd::vector<Instr> out;
    const bool have_debug_info = !proto.debug_lines.empty();
    avastd::vector<uint32_t> out_lines;
    avastd::vector<uint32_t> out_cols;
    if (have_debug_info) {
        out_lines.reserve(proto.instructions.size() * 2 + blocks.size() * 3 + 8);
        out_cols.reserve(proto.instructions.size() * 2 + blocks.size() * 3 + 8);
    }

    auto push_synth = [&](const Instr& instr) {
        out.push_back(instr);
        if (have_debug_info) { out_lines.push_back(0); out_cols.push_back(0); }
    };

    auto push_orig = [&](size_t src_idx) {
        out.push_back(proto.instructions[src_idx]);
        if (have_debug_info) {
            out_lines.push_back(src_idx < proto.debug_lines.size() ? proto.debug_lines[src_idx] : 0);
            out_cols.push_back(src_idx < proto.debug_columns.size() ? proto.debug_columns[src_idx] : 0);
        }
    };

    Instr loadk_entry{};
    loadk_entry.op = OpCode::LOADK;
    loadk_entry.a = state_reg;
    loadk_entry.b = state_const_idx[0];
    loadk_entry.c = 0;
    push_synth(loadk_entry);

    const size_t dispatcher_start = out.size();
    avastd::vector<size_t> dispatch_jmp_slot(total_states);
    for (size_t oi = 0; oi < order.size(); ++oi) {
        size_t block_id = order[oi];
        Instr eqk{};
        eqk.op = OpCode::EQK;
        eqk.a = tmp_reg;
        eqk.b = state_reg;
        eqk.c = state_const_idx[block_id];
        push_synth(eqk);

        Instr test{};
        test.op = OpCode::TEST;
        test.a = tmp_reg;
        test.b = 0;
        test.c = 1;
        push_synth(test);

        dispatch_jmp_slot[block_id] = out.size();
        Instr jmp{};
        jmp.op = OpCode::JMP;
        jmp.bx32 = 0;
        push_synth(jmp);
    }
    {
        Instr trap{};
        trap.op = OpCode::RETURN;
        trap.a = 0;
        trap.b = 0;
        trap.c = 0;
        push_synth(trap);
    }

    avastd::vector<size_t> entry_point(total_states);
    avastd::vector<size_t> deferred_jmp_slot;
    avastd::vector<size_t> deferred_target_block;

    for (size_t block_id = 0; block_id < blocks.size(); ++block_id) {
        entry_point[block_id] = out.size();
        const FlatBlock& blk = blocks[block_id];
        const bool ends_in_jmp = proto.instructions[blk.end - 1].op == OpCode::JMP;
        const bool ends_in_return = proto.instructions[blk.end - 1].op == OpCode::RETURN;
        const bool preceded_by_test = ends_in_jmp && (blk.end - blk.start) >= 2 &&
                                       proto.instructions[blk.end - 2].op == OpCode::TEST;
        const size_t body_end = ends_in_jmp ? blk.end - 1 : blk.end;

        for (size_t i = blk.start; i < body_end; ++i) {
            push_orig(i);
        }

        if (ends_in_return) {
            push_orig(blk.end - 1);
            continue;
        }

        size_t target_block_id;
        if (ends_in_jmp) {
            size_t orig_jmp_index = blk.end - 1;
            int64_t raw_target = static_cast<int64_t>(orig_jmp_index) + 1 + proto.instructions[orig_jmp_index].bx32;
            target_block_id = (raw_target >= 0 && static_cast<size_t>(raw_target) <= proto.instructions.size())
                                   ? FindBlockForTarget(blocks, static_cast<size_t>(raw_target))
                                   : exit_id;
        } else {
            target_block_id = FindBlockForTarget(blocks, blk.end);
        }

        if (preceded_by_test) {
            Instr jm{};
            jm.op = OpCode::JMP;
            jm.bx32 = 0;
            deferred_jmp_slot.push_back(out.size());
            deferred_target_block.push_back(target_block_id);
            push_synth(jm);
        } else {
            Instr lk{};
            lk.op = OpCode::LOADK;
            lk.a = state_reg;
            lk.b = state_const_idx[target_block_id];
            lk.c = 0;
            push_synth(lk);

            Instr jm{};
            jm.op = OpCode::JMP;
            jm.bx32 = static_cast<int32_t>(static_cast<int64_t>(dispatcher_start) - static_cast<int64_t>(out.size()) - 1);
            push_synth(jm);
        }
    }

    for (size_t k = 0; k < deferred_jmp_slot.size(); ++k) {
        size_t trampoline_entry = out.size();
        Instr lk{};
        lk.op = OpCode::LOADK;
        lk.a = state_reg;
        lk.b = state_const_idx[deferred_target_block[k]];
        lk.c = 0;
        push_synth(lk);

        Instr jm{};
        jm.op = OpCode::JMP;
        jm.bx32 = static_cast<int32_t>(static_cast<int64_t>(dispatcher_start) - static_cast<int64_t>(out.size()) - 1);
        push_synth(jm);

        size_t slot = deferred_jmp_slot[k];
        out[slot].bx32 = static_cast<int32_t>(static_cast<int64_t>(trampoline_entry) - static_cast<int64_t>(slot) - 1);
    }

    entry_point[exit_id] = out.size();
    {
        Instr ret{};
        ret.op = OpCode::RETURN;
        ret.a = 0;
        ret.b = 0;
        ret.c = 0;
        push_synth(ret);
    }

    for (size_t s = 0; s < total_states; ++s) {
        size_t slot = dispatch_jmp_slot[s];
        out[slot].bx32 = static_cast<int32_t>(static_cast<int64_t>(entry_point[s]) - static_cast<int64_t>(slot) - 1);
    }

    proto.instructions = avastd::move(out);
    if (have_debug_info) {
        proto.debug_lines = avastd::move(out_lines);
        proto.debug_columns = avastd::move(out_cols);
    }
    return true;
}

namespace {

void FlattenProtoTree(Proto& proto, const ObfuscateOptions& options, uint64_t& proto_counter) {
    bool eligible = options.flatten_functions.empty();
    if (!eligible) {
        for (const auto& name : options.flatten_functions) {
            if (name == proto.debug_name) { eligible = true; break; }
        }
    }
    if (eligible) {
        avastd::string tag = proto.debug_name + "#" + avastd::to_string(proto_counter);
        FlattenProtoControlFlow(proto, options.module_seed, tag);
    }
    ++proto_counter;
    for (auto& child : proto.child_protos) {
        FlattenProtoTree(*child, options, proto_counter);
    }
}

} // namespace

void ObfuscateProto(Proto& root, const ObfuscateOptions& options,
                     avastd::vector<SymbolMapEntry>* out_symbol_map) {
    if (options.flatten_control_flow) {
        uint64_t proto_counter = 0;
        FlattenProtoTree(root, options, proto_counter);
    }
    ObfuscateProtoNode(root, options, out_symbol_map);
    if (options.obfuscate_strings) {
        uint64_t next_index = 0;
        TransformStringsNode(root, options.module_seed, next_index);
    }
}

void DeobfuscateStrings(Proto& root, uint64_t module_seed) {
    uint64_t next_index = 0;
    TransformStringsNode(root, module_seed, next_index);
}

avastd::string FormatSymbolMap(const avastd::vector<SymbolMapEntry>& map) {
    avastd::ostringstream oss;
    oss << "# avalang symbol map -- guardar SOLO junto al proyecto fuente.\n";
    oss << "# NUNCA embeber este archivo en un build distribuido: revierte\n";
    oss << "# el propósito del pase de ofuscación (compiler/obfuscate.h).\n";
    oss << "# formato: kind\\toriginal\\tobfuscated\n";
    for (const auto& entry : map) {
        oss << entry.kind << '\t' << entry.original << '\t' << entry.obfuscated << '\n';
    }
    return oss.str();
}

} // namespace ava
