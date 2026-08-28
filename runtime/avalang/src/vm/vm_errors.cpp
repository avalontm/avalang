#include "vm.h"
#include "vm_internal.h"

namespace ava {

namespace {

// Escanea hacia atras, dentro del bytecode YA ejecutado de `frame`
// (indices [0, pc_limit)), buscando como se cargo por ultima vez el
// registro `reg`. Es pura lectura de instrucciones ya corridas -- no
// toca registros ni estado del VM, asi que es seguro llamarla desde un
// path de error despues de que el CALL ya fallo.
//
// Reconoce dos formas, que son las unicas que producen un nombre
// legible para el humano:
//   - GETGLOBAL reg, K[name]           -> "name"
//   - GETATTR   reg, base_reg, K[attr] -> TraceIdentifierName(base_reg) + ".attr"
//     (recursivo: encadena `Console.Foo.Bar` si hiciera falta)
//
// Cualquier otra instruccion que escriba `reg` (MOVE, aritmetica, CALL,
// etc.) corta la cadena: en ese caso el valor no vino de una carga
// directa de nombre, y devolvemos "" para caer al mensaje generico de
// siempre en vez de inventar un nombre enganoso.
avastd::string TraceIdentifierName(const CallFrame& frame, int reg, size_t pc_limit) {
    if (!frame.proto) return avastd::string();
    const auto& code = frame.proto->instructions;
    const auto& K = frame.proto->constants;
    size_t limit = pc_limit < code.size() ? pc_limit : code.size();

    for (size_t i = limit; i-- > 0; ) {
        const Instr& in = code[i];
        if (static_cast<int>(in.a) != reg) continue;

        if (in.op == OpCode::GETGLOBAL) {
            if (in.b < K.size() && K[in.b].type == ValueType::String) {
                return avastd::string(static_cast<StringObj*>(K[in.b].obj)->data);
            }
            return avastd::string();
        }
        if (in.op == OpCode::GETATTR) {
            avastd::string base = TraceIdentifierName(frame, static_cast<int>(in.b), i);
            if (base.empty()) return avastd::string();
            if (in.c < K.size() && K[in.c].type == ValueType::String) {
                return base + "." + avastd::string(static_cast<StringObj*>(K[in.c].obj)->data);
            }
            return avastd::string();
        }
        if (in.op == OpCode::MOVE) {
            // El compilador siempre inserta un MOVE justo antes de CALL
            // (ver CompileExpr/CallExpr en compiler.cpp: copia el callee a
            // un registro fresco para no pisar la variable local que lo
            // contenia) -- sin seguir este salto, la cadena se corta en
            // TODA llamada real y esta funcion nunca encuentra nada.
            return TraceIdentifierName(frame, static_cast<int>(in.b), i);
        }
        // Cualquier otra escritura a este registro rompe la cadena.
        return avastd::string();
    }
    return avastd::string();
}

} // namespace

AvaError MakeNonCallableError(VM& vm, const CallFrame& frame, const Value& callee) {
    avastd::string type_suffix =
        " (type=" + avastd::to_string(static_cast<int>(callee.type)) + ")";

    if (frame.proto && frame.pc > 0) {
        size_t call_pc = static_cast<size_t>(frame.pc - 1);
        // Guarda: MakeNonCallableError tambien se llama desde VM::Call
        // (vm_call.cpp), que puede dispararse desde codigo C++ (un
        // builtin invocando un callback) sin que `frame.pc` este parado
        // justo sobre el CALL/BASECALL que fallo. Si la instruccion en
        // esa posicion no es una de esas dos, `in.a` no es un registro
        // de callee y confiar en el daria un rastro sin sentido -- mejor
        // caer al mensaje generico.
        bool pc_is_call_site = call_pc < frame.proto->instructions.size() &&
            (frame.proto->instructions[call_pc].op == OpCode::CALL ||
             frame.proto->instructions[call_pc].op == OpCode::BASECALL);
        if (pc_is_call_site) {
            int callee_reg = static_cast<int>(frame.proto->instructions[call_pc].a);
            avastd::string name = TraceIdentifierName(frame, callee_reg, call_pc);
            if (!name.empty()) {
                avastd::string root = name;
                size_t dot = root.find('.');
                if (dot != avastd::string::npos) root = root.substr(0, dot);

                // Solo hablamos de "no definido" si la raiz realmente no
                // existe como global -- si existe pero es del tipo
                // equivocado (p.ej. `let Console = 5`), decir "no
                // definido" seria falso, asi que ahi nos quedamos con el
                // mensaje de tipo incorrecto de siempre.
                if (!vm.HasGlobal(root)) {
                    avastd::string module_path = vm.FindNativeModuleExporting(root);
                    if (!module_path.empty()) {
                        return MakeFrameError(frame,
                            "'" + name + "' is not defined -- did you forget 'import " +
                            module_path + "'?");
                    }
                    return MakeFrameError(frame, "'" + name + "' is not defined");
                }
                return MakeFrameError(frame,
                    "'" + name + "' is not callable" + type_suffix);
            }
        }
    }

    return MakeFrameError(frame, "attempt to call a non-callable value" + type_suffix);
}

void HandleFrameError(VM& vm, size_t frame_idx, const avastd::exception& e) {
    (void)vm;
    (void)frame_idx;
    (void)e;
}

AvaError VM::MakeCurrentError(const avastd::string& message) const {
    if (frames_.empty()) return AvaError(message);
    return MakeFrameError(frames_.back(), message);
}

AvaError MakeFrameError(const CallFrame& frame, const avastd::string& message) {
    int line = 0;
    int column = 0;
    if (frame.proto && frame.pc > 0) {
        size_t instr_idx = static_cast<size_t>(frame.pc - 1);
        if (instr_idx < frame.proto->debug_lines.size()) {
            line = static_cast<int>(frame.proto->debug_lines[instr_idx]);
        }
        if (instr_idx < frame.proto->debug_columns.size()) {
            column = static_cast<int>(frame.proto->debug_columns[instr_idx]);
        }
    }
    avastd::string source = frame.proto ? frame.proto->source_name : avastd::string();
    return AvaError(message, line, column, source);
}

} // namespace ava