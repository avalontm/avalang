#ifndef AVA_VM_OPCODES_H
#define AVA_VM_OPCODES_H

#include "../../platform/barekernel/stdcompat/ava_types.h"

namespace ava {

enum class OpCode : avastd::uint8_t {
    LOADK,       // A, Bx    R[A] = K[Bx]
    LOADNIL,     // A        R[A] = nil
    LOADBOOL,    // A, B     R[A] = (bool)B
    MOVE,        // A, B     R[A] = R[B]
    GETUPVAL,    // A, B     R[A] = Upval[B]
    SETUPVAL,    // A, B     Upval[B] = R[A]
    GETGLOBAL,   // A, Bx    R[A] = Globals[K[Bx]]
    SETGLOBAL,   // A, Bx    Globals[K[Bx]] = R[A]
    NEWLIST,     // A        R[A] = new empty List
    NEWDICT,     // A        R[A] = new empty Dict
    LISTAPPEND,  // A, B     R[A].append(R[B])
    GETINDEX,    // A, B, C  R[A] = R[B][R[C]]
    SETINDEX,    // A, B, C  R[A][R[B]] = R[C]
    GETATTR,     // A, B, Bx R[A] = R[B].K[Bx]
    SETATTR,     // A, Bx, C R[A].K[Bx] = R[C]
    SLICE,       // A, B, C, D R[A] = R[B][R[C]:R[D]:R[E]] (obj, start, end, step)
    ADD,         // A, B, C  R[A] = R[B] + R[C]
    SUB,         // A, B, C  R[A] = R[B] - R[C]
    MUL,         // A, B, C  R[A] = R[B] * R[C]
    DIV,         // A, B, C  R[A] = R[B] / R[C] (float)
    IDIV,        // A, B, C  R[A] = floor(R[B] / R[C]) (integer division)
    MOD,         // A, B, C  R[A] = R[B] % R[C]
    POW,         // A, B, C  R[A] = R[B] ** R[C]
    NEG,         // A, B     R[A] = -R[B]
    NOT,         // A, B     R[A] = !truthy(R[B])
    INC,         // A, B     R[A] = R[B] + 1
    DEC,         // A, B     R[A] = R[B] - 1
    EQ,          // A, B, C  R[A] = R[B] == R[C]
    EQK,         // A, B, C  R[A] = R[B] == K[C]
    NEK,         // A, B, C  R[A] = R[B] != K[C]
    NE,          // A, B, C  R[A] = R[B] != R[C]
    LT,          // A, B, C  R[A] = R[B] < R[C]
    LE,          // A, B, C  R[A] = R[B] <= R[C]
    GT,          // A, B, C  R[A] = R[B] > R[C]
    GE,          // A, B, C  R[A] = R[B] >= R[C]
    JMP,         // sBx(32)  pc += sBx (32-bit signed offset)
    TEST,        // A, C     if truthy(R[A]) != C then pc++ (skips next JMP)
    CALL,        // A, B, C  R[A..] = call R[A](R[A+1..A+B-1]), C results
    RETURN,      // A, B     return R[A..A+B-1]
    CLOSURE,     // A, Bx    R[A] = make closure from child_protos[Bx]
    NEWCLASS,    // A, Bx    R[A] = new Class from class template K[Bx]
    NEWINSTANCE, // A, Bx    R[A] = new Instance of class R[B]
    BASECALL,    // A, B, C  call base class method: R[A].method(K[B]) with args R[A+1..A+C]
    YIELD,       // A, B     suspend coroutine, yielding R[A..A+B-1]
    RESUME,      // A, B, C  resume coroutine R[B] with args, C results
    AWAIT,       // A        await Task in R[A]; suspends until settled, R[A] = result
    TRY,         // sBx(32)  push handler, jump target (32-bit)
    TRY_END,     //           mark end of try block
    CATCH,       // sBx(32)  if exception active, jump (32-bit)
    RAISE,       // A         raise exception from R[A]
    ARGC,        // A        R[A] = number of args the current call actually received (Number)
};

// iABC-style fixed-width instruction, same family as Lua's own encoding.
// Bx / sBx reuse the B+C fields as one wider operand where noted above.
struct Instr {
    OpCode  op;
    avastd::uint8_t a;
    union {
        struct {
            avastd::uint16_t b; // low 16 bits
            avastd::uint16_t c; // high 16 bits (extends b for 32-bit signed offsets)
        };
        avastd::int32_t bx32; // 32-bit signed offset (used by JMP, TRY, CATCH)
    };
};

} // namespace ava

#endif // AVA_VM_OPCODES_H
