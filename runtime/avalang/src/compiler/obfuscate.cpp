#include "obfuscate.h"
#include <sstream>
#include <iomanip>
#include <set>
#include <algorithm>

namespace ava {

namespace {

// FNV-1a de 64 bits, sembrado con module_seed. No es un hash criptográfico
// y no pretende serlo: el objetivo acá es solo generar identificadores
// estables y sin colisiones prácticas para nombres de función dentro de un
// mismo build, no resistir un adversario que intente invertirlo -- para
// alguien mirando el bytecode ofuscado sin el mapa de símbolos, un
// identificador opaco cumple su función igual de bien con FNV que con
// SHA-256, y evita que compiler/ (que no debería saber nada de
// criptografía) dependa de runtime/avapack/src/checksum.
uint64_t HashSymbol(uint64_t seed, const std::string& kind, const std::string& value) {
    uint64_t h = 1469598103934665603ull ^ seed;
    auto mix = [&h](const std::string& s) {
        for (unsigned char c : s) {
            h ^= c;
            h *= 1099511628211ull;
        }
        h ^= 0xFFu; // separador entre "kind" y "value" para no colisionar
                    // p.ej. HashSymbol(seed, "fu", "nc") con
                    // HashSymbol(seed, "func", "")
        h *= 1099511628211ull;
    };
    mix(kind);
    mix(value);
    return h;
}

std::string ToOpaqueId(uint64_t hash) {
    std::ostringstream oss;
    oss << "sym_" << std::hex << std::setw(16) << std::setfill('0') << hash;
    return oss.str();
}

// Genera `len` bytes de keystream determinista a partir de (seed, string_index),
// hasheando bloques consecutivos (contador de bloque incluido en el hash) hasta
// cubrir la longitud pedida. XOR con este keystream es su propia inversa, así que
// la misma función sirve para ofuscar y para revertir -- no hace falta una
// implementación separada de "decode".
std::string XorKeystreamTransform(uint64_t seed, uint64_t string_index, const std::string& data) {
    std::string out;
    out.resize(data.size());
    uint32_t block = 0;
    size_t pos = 0;
    while (pos < data.size()) {
        std::ostringstream tag;
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

// Recorre proto->constants buscando Value::String y les aplica
// XorKeystreamTransform in-place, avanzando `next_index` (compartido entre todo
// el árbol de Proto, pasado por referencia) para que cada string del proyecto
// -- sin importar en qué función/child_proto esté -- tenga un índice único y
// por lo tanto un keystream distinto. El orden de recorrido (constants en orden,
// luego child_protos en orden) es el mismo tanto al ofuscar como al
// deofuscar porque ambos caminan la misma estructura de datos ya construida.
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

// -- Parte 3: control-flow flattening --------------------------------------

struct FlatBlock {
    size_t start; // original instruction index, inclusive
    size_t end;   // original instruction index, exclusive
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

// Leaders: instr 0, cualquier target de JMP, la instruccion siguiente a
// cada JMP, y la instruccion siguiente a cada RETURN. TEST nunca es leader
// por si solo -- siempre va inmediatamente seguido de un JMP (protocolo del
// VM), asi que el JMP que le sigue ya cubre el corte de bloque necesario.
std::vector<FlatBlock> ComputeBlocks(const std::vector<Instr>& ins) {
    std::set<size_t> leaders;
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
    std::vector<size_t> sorted(leaders.begin(), leaders.end());
    std::vector<FlatBlock> blocks;
    for (size_t k = 0; k < sorted.size(); ++k) {
        size_t start = sorted[k];
        size_t end = (k + 1 < sorted.size()) ? sorted[k + 1] : ins.size();
        if (start < end) blocks.push_back(FlatBlock{start, end});
    }
    return blocks;
}

// blocks.size() (fuera de rango) = sentinel "exit": el target cae en
// ins.size() exacto (JMP al final de la funcion) o en algo que no es
// leader de ningun bloque real.
size_t FindBlockForTarget(const std::vector<FlatBlock>& blocks, size_t target) {
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (blocks[i].start == target) return i;
    }
    return blocks.size();
}

uint64_t NextPrng(uint64_t seed, const std::string& tag, uint64_t& counter) {
    return HashSymbol(seed, "cf_prng", tag + ":" + std::to_string(counter++));
}

} // namespace (helpers de flatten cierran aca, el resto del anonymous
  // namespace original sigue mas abajo sin cambios)

namespace {

void ObfuscateProtoNode(Proto& proto, const ObfuscateOptions& options,
                         std::vector<SymbolMapEntry>* out_symbol_map) {
    if (!proto.debug_name.empty()) {
        std::string obfuscated = ToOpaqueId(HashSymbol(options.module_seed, "function", proto.debug_name));
        if (out_symbol_map) {
            out_symbol_map->push_back(SymbolMapEntry{"function", proto.debug_name, obfuscated});
        }
        proto.debug_name = obfuscated;
    }

    if (!proto.source_name.empty()) {
        std::string obfuscated = ToOpaqueId(HashSymbol(options.module_seed, "source_file", proto.source_name));
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

bool FlattenProtoControlFlow(Proto& proto, uint64_t seed, const std::string& tag) {
    if (proto.instructions.size() < 2) return false;
    if (HasUnsupportedControlFlow(proto)) return false;

    std::vector<FlatBlock> blocks = ComputeBlocks(proto.instructions);
    if (blocks.size() < 2) return false;

    const size_t exit_id = blocks.size();
    const size_t total_states = blocks.size() + 1;

    std::vector<uint16_t> state_const_idx(total_states);
    for (size_t s = 0; s < total_states; ++s) {
        uint64_t state_value = HashSymbol(seed, "cf_state", tag + ":" + std::to_string(s)) & 0xFFFFFFFFFFFFull;
        state_const_idx[s] = static_cast<uint16_t>(proto.constants.size());
        proto.constants.push_back(Value::Number(static_cast<double>(state_value)));
    }

    // El dispatcher necesita una rama EQK/TEST/JMP por cada estado posible,
    // incluido exit_id -- si no, un JMP hacia "exit" (ver ends_in_jmp mas
    // abajo, caso raw_target fuera de rango) no tendria a donde saltar
    // dentro del dispatcher y el patch final terminaria escribiendo sobre
    // el slot 0 (bug real, encontrado corriendo flatten_check.cpp contra
    // un if/else de prueba: el LOADK inicial quedaba pisado y la funcion
    // devolvia el resultado del RETURN de "estado desconocido").
    std::vector<size_t> order(total_states);
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    uint64_t shuffle_counter = 0;
    for (size_t i = order.size(); i > 1; --i) {
        uint64_t r = NextPrng(seed, tag, shuffle_counter);
        size_t j = static_cast<size_t>(r % i);
        std::swap(order[i - 1], order[j]);
    }

    const uint16_t state_reg = proto.num_registers;
    const uint16_t tmp_reg = static_cast<uint16_t>(proto.num_registers + 1);
    proto.num_registers = static_cast<uint16_t>(proto.num_registers + 2);

    std::vector<Instr> out;
    out.reserve(proto.instructions.size() * 2 + blocks.size() * 3 + 8);

    Instr loadk_entry{};
    loadk_entry.op = OpCode::LOADK;
    loadk_entry.a = state_reg;
    loadk_entry.b = state_const_idx[0];
    loadk_entry.c = 0;
    out.push_back(loadk_entry);

    const size_t dispatcher_start = out.size();
    std::vector<size_t> dispatch_jmp_slot(total_states);
    for (size_t oi = 0; oi < order.size(); ++oi) {
        size_t block_id = order[oi];
        Instr eqk{};
        eqk.op = OpCode::EQK;
        eqk.a = tmp_reg;
        eqk.b = state_reg;
        eqk.c = state_const_idx[block_id];
        out.push_back(eqk);

        Instr test{};
        test.op = OpCode::TEST;
        test.a = tmp_reg;
        test.b = 0;
        test.c = 1;
        out.push_back(test);

        dispatch_jmp_slot[block_id] = out.size();
        Instr jmp{};
        jmp.op = OpCode::JMP;
        jmp.bx32 = 0;
        out.push_back(jmp);
    }
    {
        // Estado desconocido (no deberia poder pasar con bytecode propio,
        // pero un dispatcher sin salida por defecto es peor que uno con
        // un RETURN 0,0 -- mismo patron que compiler.cpp usa para return
        // implicito).
        Instr trap{};
        trap.op = OpCode::RETURN;
        trap.a = 0;
        trap.b = 0;
        trap.c = 0;
        out.push_back(trap);
    }

    std::vector<size_t> entry_point(total_states);
    // TEST solo salta UNA instruccion (el JMP que le sigue -- protocolo
    // fijo del VM, ver opcodes.h). Si un bloque termina en TEST+JMP no se
    // puede reemplazar ese JMP por dos instrucciones en linea (LOADK+JMP):
    // el camino donde TEST decide saltar solo se saltaria la primera
    // (LOADK) y caeria en la segunda (JMP) igual, con el registro de
    // estado sin actualizar -- exactamente el bug que encontro
    // flatten_check.cpp (loop infinito entre el dispatcher y el bloque
    // con el registro de estado nunca cambiando). Para esos casos se dej
    // un JMP real, unico, inmediatamente despues de TEST, apuntando a un
    // trampolin (LOADK+JMP-al-dispatcher) emitido aparte, con el offset
    // parcheado en una segunda pasada una vez se conoce su posicion.
    std::vector<size_t> deferred_jmp_slot;
    std::vector<size_t> deferred_target_block;

    for (size_t block_id = 0; block_id < blocks.size(); ++block_id) {
        entry_point[block_id] = out.size();
        const FlatBlock& blk = blocks[block_id];
        const bool ends_in_jmp = proto.instructions[blk.end - 1].op == OpCode::JMP;
        const bool ends_in_return = proto.instructions[blk.end - 1].op == OpCode::RETURN;
        const bool preceded_by_test = ends_in_jmp && (blk.end - blk.start) >= 2 &&
                                       proto.instructions[blk.end - 2].op == OpCode::TEST;
        const size_t body_end = ends_in_jmp ? blk.end - 1 : blk.end;

        for (size_t i = blk.start; i < body_end; ++i) {
            out.push_back(proto.instructions[i]);
        }

        if (ends_in_return) {
            out.push_back(proto.instructions[blk.end - 1]);
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
            // Bloque sin terminador propio (cae en el por ser leader de
            // otro JMP externo, no porque este termine en JMP/RETURN):
            // el sucesor logico es siempre el bloque contiguo siguiente.
            target_block_id = FindBlockForTarget(blocks, blk.end);
        }

        if (preceded_by_test) {
            Instr jm{};
            jm.op = OpCode::JMP;
            jm.bx32 = 0;
            deferred_jmp_slot.push_back(out.size());
            deferred_target_block.push_back(target_block_id);
            out.push_back(jm);
        } else {
            Instr lk{};
            lk.op = OpCode::LOADK;
            lk.a = state_reg;
            lk.b = state_const_idx[target_block_id];
            lk.c = 0;
            out.push_back(lk);

            Instr jm{};
            jm.op = OpCode::JMP;
            jm.bx32 = static_cast<int32_t>(static_cast<int64_t>(dispatcher_start) - static_cast<int64_t>(out.size()) - 1);
            out.push_back(jm);
        }
    }

    // Trampolines de los casos TEST+JMP diferidos arriba: cada uno vive
    // aparte (no en linea) para no romper el "TEST salta solo 1 instruccion".
    for (size_t k = 0; k < deferred_jmp_slot.size(); ++k) {
        size_t trampoline_entry = out.size();
        Instr lk{};
        lk.op = OpCode::LOADK;
        lk.a = state_reg;
        lk.b = state_const_idx[deferred_target_block[k]];
        lk.c = 0;
        out.push_back(lk);

        Instr jm{};
        jm.op = OpCode::JMP;
        jm.bx32 = static_cast<int32_t>(static_cast<int64_t>(dispatcher_start) - static_cast<int64_t>(out.size()) - 1);
        out.push_back(jm);

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
        out.push_back(ret);
    }

    for (size_t s = 0; s < total_states; ++s) {
        size_t slot = dispatch_jmp_slot[s];
        out[slot].bx32 = static_cast<int32_t>(static_cast<int64_t>(entry_point[s]) - static_cast<int64_t>(slot) - 1);
    }

    proto.instructions = std::move(out);
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
        std::string tag = proto.debug_name + "#" + std::to_string(proto_counter);
        FlattenProtoControlFlow(proto, options.module_seed, tag);
    }
    ++proto_counter;
    for (auto& child : proto.child_protos) {
        FlattenProtoTree(*child, options, proto_counter);
    }
}

} // namespace

void ObfuscateProto(Proto& root, const ObfuscateOptions& options,
                     std::vector<SymbolMapEntry>* out_symbol_map) {
    if (options.flatten_control_flow) {
        // Corre ANTES del renombrado de simbolos (mas abajo): flatten
        // matchea flatten_functions contra debug_name original, y no lee
        // ni escribe debug_name/source_name, asi que el orden entre este
        // paso y ObfuscateProtoNode no afecta el resultado del renombrado
        // -- solo importa para que el matching de flatten_functions vea
        // los nombres reales, no los ya ofuscados.
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
    // XOR con el mismo keystream es su propia inversa: es literalmente el
    // mismo recorrido que TransformStringsNode usa para ofuscar. Se separa
    // en una función pública propia (en vez de reusar ObfuscateProto con
    // un flag "reverse") porque el caller de este lado (avapack/avahost al
    // cargar un .avbc) no tiene ni debería tener que pasar un
    // ObfuscateOptions completo -- solo necesita el seed.
    uint64_t next_index = 0;
    TransformStringsNode(root, module_seed, next_index);
}

std::string FormatSymbolMap(const std::vector<SymbolMapEntry>& map) {
    std::ostringstream oss;
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
