// Test manual (no framework) para proto_io + obfuscate. Construye un
// Proto a mano (mismo patrón que usa el resto del repo para ejercitar el
// VM sin el frontend ANTLR -- ver frontend_stub.cpp) que simula una
// función pequeña con un hijo (closure), lo ofusca, lo serializa, lo
// deserializa, y verifica que el resultado sea funcionalmente idéntico al
// original salvo por los campos de debug.
#include <cassert>
#include <iostream>
#include <sstream>
#include "proto_io.h"
#include "obfuscate.h"

using namespace ava;

namespace {

std::shared_ptr<Proto> BuildSampleProto() {
    auto child = std::make_shared<Proto>();
    child->num_registers = 3;
    child->num_params = 1;
    child->is_vararg = false;
    child->is_method = false;
    child->constants.push_back(Value::Number(42));
    child->constants.push_back(Value::String("hola mundo"));
    child->instructions.push_back(Instr{OpCode::LOADK, 1, {0}});
    child->instructions.push_back(Instr{OpCode::RETURN, 1, {1}});
    child->debug_lines = {10, 11};
    child->debug_name = "saludo_interno";
    child->source_name = "services/catalog.ava";

    auto root = std::make_shared<Proto>();
    root->num_registers = 4;
    root->num_params = 0;
    root->is_vararg = true;
    root->constants.push_back(Value::Bool(true));
    root->constants.push_back(Value::Nil());
    root->instructions.push_back(Instr{OpCode::CLOSURE, 0, {0}});
    root->instructions.push_back(Instr{OpCode::CALL, 0, {1, 0}});
    root->upvalue_descs.push_back(UpvalDesc{true, 2});
    root->child_protos.push_back(child);
    root->debug_lines = {1, 2};
    root->debug_name = "main";
    root->source_name = "app.ava";
    return root;
}

bool ProtosEqualIgnoringDebug(const Proto& a, const Proto& b) {
    if (a.num_registers != b.num_registers) return false;
    if (a.num_params != b.num_params) return false;
    if (a.is_vararg != b.is_vararg) return false;
    if (a.is_method != b.is_method) return false;
    if (a.constants.size() != b.constants.size()) return false;
    for (size_t i = 0; i < a.constants.size(); ++i) {
        const auto& ca = a.constants[i];
        const auto& cb = b.constants[i];
        if (ca.type != cb.type) return false;
        switch (ca.type) {
            case ValueType::Bool:   if (ca.b != cb.b) return false; break;
            case ValueType::Number: if (ca.n != cb.n) return false; break;
            case ValueType::String:
                if (static_cast<StringObj*>(ca.obj)->data != static_cast<StringObj*>(cb.obj)->data)
                    return false;
                break;
            default: break;
        }
    }
    if (a.instructions.size() != b.instructions.size()) return false;
    for (size_t i = 0; i < a.instructions.size(); ++i) {
        if (a.instructions[i].op != b.instructions[i].op) return false;
        if (a.instructions[i].a != b.instructions[i].a) return false;
        if (a.instructions[i].b != b.instructions[i].b) return false;
        if (a.instructions[i].c != b.instructions[i].c) return false;
    }
    if (a.upvalue_descs.size() != b.upvalue_descs.size()) return false;
    for (size_t i = 0; i < a.upvalue_descs.size(); ++i) {
        if (a.upvalue_descs[i].from_parent_local != b.upvalue_descs[i].from_parent_local) return false;
        if (a.upvalue_descs[i].index != b.upvalue_descs[i].index) return false;
    }
    if (a.child_protos.size() != b.child_protos.size()) return false;
    for (size_t i = 0; i < a.child_protos.size(); ++i) {
        if (!ProtosEqualIgnoringDebug(*a.child_protos[i], *b.child_protos[i])) return false;
    }
    return true;
}

} // namespace

int main() {
    // --- 1. Round-trip de serialización SIN ofuscar (con debug info) ---
    {
        auto proto = BuildSampleProto();
        auto bytes = SerializeProto(*proto);
        std::string err;
        auto restored = DeserializeProto(bytes, &err);
        assert(restored != nullptr && err.empty());
        assert(ProtosEqualIgnoringDebug(*proto, *restored));
        assert(restored->debug_name == "main");
        assert(restored->source_name == "app.ava");
        assert(restored->child_protos[0]->debug_name == "saludo_interno");
        assert(restored->debug_lines.size() == 2);
        std::cout << "[OK] round-trip sin ofuscar preserva bytecode + debug info\n";
    }

    // --- 2. Ofuscar y verificar que cambian los símbolos, no el bytecode ---
    {
        auto proto = BuildSampleProto();
        std::vector<SymbolMapEntry> map;
        ObfuscateOptions opts;
        opts.module_seed = 0xC0FFEEULL;
        opts.strip_debug_lines = true;
        ObfuscateProto(*proto, opts, &map);

        assert(proto->debug_name != "main");
        assert(proto->debug_name.rfind("sym_", 0) == 0);
        assert(proto->source_name != "app.ava");
        assert(proto->child_protos[0]->debug_name != "saludo_interno");
        assert(proto->debug_lines.empty());
        assert(proto->child_protos[0]->debug_lines.empty());
        // El bytecode/constant pool NO debe tocarse en esta parte del pase.
        assert(proto->constants.size() == 2);
        assert(proto->child_protos[0]->constants.size() == 2);
        assert(static_cast<StringObj*>(proto->child_protos[0]->constants[1].obj)->data == "hola mundo");

        // 4 entradas: main, app.ava, saludo_interno, services/catalog.ava
        assert(map.size() == 4);

        std::cout << "[OK] ObfuscateProto reemplaza symbols, preserva bytecode/constants\n";

        // --- 3. Determinismo por seed: misma seed -> mismo id ofuscado ---
        auto proto2 = BuildSampleProto();
        std::vector<SymbolMapEntry> map2;
        ObfuscateProto(*proto2, opts, &map2);
        assert(proto->debug_name == proto2->debug_name);
        assert(proto->child_protos[0]->debug_name == proto2->child_protos[0]->debug_name);
        std::cout << "[OK] misma module_seed produce los mismos IDs ofuscados (reproducible)\n";

        // --- 4. Seeds distintas -> ids distintos (no hay tabla fija) ---
        auto proto3 = BuildSampleProto();
        std::vector<SymbolMapEntry> map3;
        ObfuscateOptions opts2;
        opts2.module_seed = 0xDEADBEEFULL;
        ObfuscateProto(*proto3, opts2, &map3);
        assert(proto->debug_name != proto3->debug_name);
        std::cout << "[OK] seeds distintas producen IDs distintos (nada hardcodeado)\n";

        // --- 5. Round-trip serializando el proto YA ofuscado ---
        auto bytes = SerializeProto(*proto);
        std::string err;
        auto restored = DeserializeProto(bytes, &err);
        assert(restored != nullptr && err.empty());
        assert(ProtosEqualIgnoringDebug(*proto, *restored));
        assert(restored->debug_name == proto->debug_name);
        std::cout << "[OK] round-trip de un Proto ya ofuscado preserva todo\n";

        // --- 6. strip_debug_info en WriteProto: ni siquiera queda el ID opaco ---
        ProtoIoOptions io_opts;
        io_opts.strip_debug_info = true;
        auto bytes_no_debug = SerializeProto(*proto, io_opts);
        auto restored_no_debug = DeserializeProto(bytes_no_debug, &err);
        assert(restored_no_debug != nullptr);
        assert(restored_no_debug->debug_name.empty());
        assert(restored_no_debug->source_name.empty());
        assert(restored_no_debug->debug_lines.empty());
        assert(ProtosEqualIgnoringDebug(*proto, *restored_no_debug));
        std::cout << "[OK] strip_debug_info en WriteProto omite símbolos por completo del .avbc\n";

        std::cout << "\nmapa de símbolos generado (queda SOLO del lado del dev, nunca en el build):\n";
        std::cout << FormatSymbolMap(map);
    }

    // --- 7. Parte 2: ofuscar strings, incluyendo nombres de GETGLOBAL/GETATTR ---
    {
        auto proto = std::make_shared<Proto>();
        proto->num_registers = 3;
        // K[0] = nombre de global ("Kernel"), K[1] = mensaje de usuario,
        // K[2] = nombre de atributo ("saldo") -- las tres son
        // Value::String en la misma constant pool, tal como las usaría
        // GETGLOBAL/GETATTR en bytecode real (ver opcodes.h).
        proto->constants.push_back(Value::String("Kernel"));
        proto->constants.push_back(Value::String("Bienvenido a AvaLang"));
        proto->constants.push_back(Value::String("saldo"));
        proto->instructions.push_back(Instr{OpCode::GETGLOBAL, 0, {0}});
        proto->instructions.push_back(Instr{OpCode::GETATTR, 1, {0, 2}});

        auto child = std::make_shared<Proto>();
        child->constants.push_back(Value::String("otro string en un child_proto"));
        proto->child_protos.push_back(child);

        std::string original_0 = static_cast<StringObj*>(proto->constants[0].obj)->data;
        std::string original_1 = static_cast<StringObj*>(proto->constants[1].obj)->data;
        std::string original_2 = static_cast<StringObj*>(proto->constants[2].obj)->data;
        std::string original_child = static_cast<StringObj*>(proto->child_protos[0]->constants[0].obj)->data;

        ObfuscateOptions opts;
        opts.module_seed = 0x1234567890ABCDEFULL;
        opts.obfuscate_strings = true;
        opts.strip_debug_lines = false; // no relevante acá, no ensuciar el test
        ObfuscateProto(*proto, opts, nullptr);

        // Los strings YA NO son los originales tras ofuscar.
        assert(static_cast<StringObj*>(proto->constants[0].obj)->data != original_0);
        assert(static_cast<StringObj*>(proto->constants[1].obj)->data != original_1);
        assert(static_cast<StringObj*>(proto->constants[2].obj)->data != original_2);
        assert(static_cast<StringObj*>(proto->child_protos[0]->constants[0].obj)->data != original_child);
        std::cout << "[OK] ObfuscateProto(obfuscate_strings=true) transforma todos los strings, incluidos child_protos\n";

        // Serializar/deserializar el proto YA ofuscado (simula: build ->
        // .avbc en disco -> avahost lo carga en otra corrida del proceso).
        auto bytes = SerializeProto(*proto);
        std::string err;
        auto loaded = DeserializeProto(bytes, &err);
        assert(loaded != nullptr && err.empty());

        // El loader (avahost) llama DeobfuscateStrings con el mismo seed
        // ANTES de correr el módulo en el VM.
        DeobfuscateStrings(*loaded, opts.module_seed);

        assert(static_cast<StringObj*>(loaded->constants[0].obj)->data == original_0);
        assert(static_cast<StringObj*>(loaded->constants[1].obj)->data == original_1);
        assert(static_cast<StringObj*>(loaded->constants[2].obj)->data == original_2);
        assert(static_cast<StringObj*>(loaded->child_protos[0]->constants[0].obj)->data == original_child);
        std::cout << "[OK] DeobfuscateStrings revierte exactamente: \"" 
                   << static_cast<StringObj*>(loaded->constants[0].obj)->data
                   << "\" recuperado -- GETGLOBAL resolvería el nombre real, no el ofuscado\n";

        // Seed equivocado (ej. build corrupto/mezcla de versiones) NO debe
        // reproducir el string original -- si esto fallara, el "cifrado"
        // no estaría aportando nada.
        auto loaded_wrong = DeserializeProto(bytes, &err);
        DeobfuscateStrings(*loaded_wrong, opts.module_seed ^ 0xFFULL);
        assert(static_cast<StringObj*>(loaded_wrong->constants[0].obj)->data != original_0);
        std::cout << "[OK] seed incorrecto no revierte al string original (no es un XOR con clave fija adivinable)\n";
    }

    // --- 8. Archivo corrupto / no-avbc se rechaza sin crashear ---
    {
        std::string garbage = "esto no es un .avbc";
        std::istringstream iss(garbage, std::ios::binary);
        std::string err;
        auto result = ReadProto(iss, &err);
        assert(result == nullptr);
        assert(!err.empty());
        std::cout << "[OK] input inválido se rechaza limpiamente (\"" << err << "\")\n";
    }

    std::cout << "\nTODOS LOS TESTS PASARON\n";
    return 0;
}
