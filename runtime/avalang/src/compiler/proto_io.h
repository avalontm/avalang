#ifndef AVA_COMPILER_PROTO_IO_H
#define AVA_COMPILER_PROTO_IO_H

#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"
#include "../vm/proto.h"

namespace ava {

// Formato binario ".avbc" para un árbol de Proto ya compilado.
//
// Objetivo: poder distribuir el resultado de compilar un proyecto avalang
// sin embeber el .ava en texto plano y sin depender de cifrado simétrico
// para ocultarlo (ver runtime/avapack/README.md, sección Fase 6, motivo
// del cambio de arquitectura). Un .avbc es la salida normal de un paso de
// compilación, análogo a un .pyc de Python o a un chunk de luac -- se
// puede inspeccionar con herramientas dedicadas, pero no es código fuente
// legible ni requiere reconstruir una clave en memoria para poder correrlo.
//
// El formato NO es un mecanismo de ofuscación en sí mismo -- ver
// compiler/obfuscate.h para eso. proto_io solo serializa lo que ya está en
// el Proto (que puede o no haber pasado por ObfuscateProto antes).
//
// Layout (little-endian):
//   magic    : 4 bytes  "AVBC"
//   version  : u16      formato actual = 1
//   flags    : u16      bit0 = has_debug_info (ver WriteOptions)
//   <proto>  : nodo raíz, formato recursivo (ver abajo)
//
// Nodo <proto>:
//   num_registers : u16
//   num_params    : u8
//   is_vararg     : u8 (bool)
//   is_method     : u8 (bool)
//   const_count   : u32
//     const_count * <value>
//   instr_count   : u32
//     instr_count * <instr>  (7 bytes: op:u8, a:u8, b:u16, c:u16)
//   upval_count   : u32
//     upval_count * (from_parent_local:u8, index:u16)
//   child_count   : u32
//     child_count * <proto>  (recursivo)
//   [si flags.has_debug_info]:
//     debug_line_count : u32
//       debug_line_count * u32
//     debug_name   : <string>
//     source_name  : <string>
//
// Nodo <value> (constant pool -- solo tipos válidos como constante
// compilada: Nil/Bool/Number/String; cualquier otro ValueType en
// Proto::constants es un bug del compilador, no algo que este formato deba
// soportar):
//   tag : u8   (0=Nil, 1=Bool, 2=Number, 3=String)
//   payload según tag (Bool: u8, Number: f64 LE, String: <string>)
//
// Nodo <string>:
//   len  : u32
//   len bytes UTF-8, sin terminador nulo

struct ProtoIoOptions {
    // Si es false, no se escriben debug_lines/debug_name/source_name.
    // Un .avbc sin debug info no puede producir stack traces con nombres
    // de función ni líneas de fuente -- se usa para builds de release
    // ofuscados donde eso es intencional (ver obfuscate.h, que ya limpia
    // estos campos antes; strip_debug_info es un segundo cinturón por si
    // se serializa un Proto que no pasó por el pase de ofuscación).
    bool strip_debug_info = false;
};

// Serializa `root` (y todo su árbol de child_protos) a `out`.
// Devuelve false si `out` queda en mal estado (fallo de escritura).
bool WriteProto(const Proto& root, avastd::ostream& out,
                 const ProtoIoOptions& options = ProtoIoOptions());

// Conveniencia: serializa a un buffer en memoria.
avastd::vector<uint8_t> SerializeProto(const Proto& root,
                                     const ProtoIoOptions& options = ProtoIoOptions());

// Resultado de ReadProto: el Proto reconstruido, o nullptr si `in` no
// contiene un .avbc válido (magic/version incorrectos, stream truncado,
// etc). `error_out`, si no es null, recibe una descripción corta del
// motivo del fallo -- pensado para mensajes de error de avahost/avacli,
// no para lógica de negocio (no hay códigos de error estables todavía).
avastd::shared_ptr<Proto> ReadProto(avastd::istream& in, avastd::string* error_out = nullptr);

avastd::shared_ptr<Proto> DeserializeProto(const avastd::vector<uint8_t>& bytes,
                                         avastd::string* error_out = nullptr);

} // namespace ava

#endif // AVA_COMPILER_PROTO_IO_H
