#include "proto_io.h"
#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

namespace ava {

namespace {

constexpr char kMagic[4] = {'A', 'V', 'B', 'C'};
constexpr uint16_t kVersion = 1;
constexpr uint16_t kFlagHasDebugInfo = 1u << 0;

void WriteU8(avastd::ostream& out, uint8_t v) {
    out.put(static_cast<char>(v));
}

void WriteU16(avastd::ostream& out, uint16_t v) {
    unsigned char buf[2] = {
        static_cast<unsigned char>(v & 0xFF),
        static_cast<unsigned char>((v >> 8) & 0xFF),
    };
    out.write(reinterpret_cast<const char*>(buf), 2);
}

void WriteU32(avastd::ostream& out, uint32_t v) {
    unsigned char buf[4] = {
        static_cast<unsigned char>(v & 0xFF),
        static_cast<unsigned char>((v >> 8) & 0xFF),
        static_cast<unsigned char>((v >> 16) & 0xFF),
        static_cast<unsigned char>((v >> 24) & 0xFF),
    };
    out.write(reinterpret_cast<const char*>(buf), 4);
}

void WriteF64(avastd::ostream& out, double v) {
    unsigned char buf[8];
    avastd::memcpy(buf, &v, 8); // host is little-endian on all avalang build targets
    out.write(reinterpret_cast<const char*>(buf), 8);
}

void WriteString(avastd::ostream& out, const avastd::string& s) {
    WriteU32(out, static_cast<uint32_t>(s.size()));
    if (!s.empty()) out.write(s.data(), static_cast<avastd::streamsize>(s.size()));
}

bool ReadU8(avastd::istream& in, uint8_t& v) {
    int c = in.get();
    if (c == -1) return false;
    v = static_cast<uint8_t>(c);
    return true;
}

bool ReadU16(avastd::istream& in, uint16_t& v) {
    unsigned char buf[2];
    if (!in.read(reinterpret_cast<char*>(buf), 2)) return false;
    v = static_cast<uint16_t>(buf[0] | (buf[1] << 8));
    return true;
}

bool ReadU32(avastd::istream& in, uint32_t& v) {
    unsigned char buf[4];
    if (!in.read(reinterpret_cast<char*>(buf), 4)) return false;
    v = static_cast<uint32_t>(buf[0]) | (static_cast<uint32_t>(buf[1]) << 8) |
        (static_cast<uint32_t>(buf[2]) << 16) | (static_cast<uint32_t>(buf[3]) << 24);
    return true;
}

bool ReadF64(avastd::istream& in, double& v) {
    unsigned char buf[8];
    if (!in.read(reinterpret_cast<char*>(buf), 8)) return false;
    avastd::memcpy(&v, buf, 8);
    return true;
}

bool ReadString(avastd::istream& in, avastd::string& s) {
    uint32_t len = 0;
    if (!ReadU32(in, len)) return false;
    // Guard against a corrupt/truncated length blowing up the allocation;
    // no .avbc this compiler emits should ever need a single string this
    // large (source files aren't gigabytes), so treat it as invalid input
    // rather than trusting it blindly.
    constexpr uint32_t kMaxReasonableStringLen = 64u * 1024u * 1024u;
    if (len > kMaxReasonableStringLen) return false;
    s.resize(len);
    if (len == 0) return true;
    return static_cast<bool>(in.read(&s[0], static_cast<avastd::streamsize>(len)));
}

void WriteValue(avastd::ostream& out, const Value& v) {
    switch (v.type) {
        case ValueType::Nil:
            WriteU8(out, 0);
            break;
        case ValueType::Bool:
            WriteU8(out, 1);
            WriteU8(out, v.b ? 1 : 0);
            break;
        case ValueType::Number:
            WriteU8(out, 2);
            WriteF64(out, v.n);
            break;
        case ValueType::String: {
            WriteU8(out, 3);
            const auto* str_obj = static_cast<const StringObj*>(v.obj);
            WriteString(out, str_obj ? str_obj->data : avastd::string());
            break;
        }
        default:
            // No debería llegar acá: el compilador solo mete Nil/Bool/
            // Number/String en la constant pool (ver AddConstant en
            // compiler.cpp). Si esto dispara, algo nuevo está agregando
            // un tipo de constante que este formato todavía no sabe
            // serializar -- se escribe como Nil en vez de corromper el
            // stream, para que el .avbc siga siendo válido aunque pierda
            // esa constante puntual.
            WriteU8(out, 0);
            break;
    }
}

bool ReadValue(avastd::istream& in, Value& out_value) {
    uint8_t tag = 0;
    if (!ReadU8(in, tag)) return false;
    switch (tag) {
        case 0:
            out_value = Value::Nil();
            return true;
        case 1: {
            uint8_t b = 0;
            if (!ReadU8(in, b)) return false;
            out_value = Value::Bool(b != 0);
            return true;
        }
        case 2: {
            double d = 0;
            if (!ReadF64(in, d)) return false;
            out_value = Value::Number(d);
            return true;
        }
        case 3: {
            avastd::string s;
            if (!ReadString(in, s)) return false;
            out_value = Value::String(s);
            return true;
        }
        default:
            return false; // tag desconocido: archivo corrupto o de una versión futura
    }
}

void WriteProtoNode(const Proto& proto, avastd::ostream& out, bool with_debug_info) {
    WriteU16(out, proto.num_registers);
    WriteU8(out, proto.num_params);
    WriteU8(out, proto.is_vararg ? 1 : 0);
    WriteU8(out, proto.is_method ? 1 : 0);

    WriteU32(out, static_cast<uint32_t>(proto.constants.size()));
    for (const auto& c : proto.constants) WriteValue(out, c);

    WriteU32(out, static_cast<uint32_t>(proto.instructions.size()));
    for (const auto& instr : proto.instructions) {
        WriteU8(out, static_cast<uint8_t>(instr.op));
        WriteU8(out, instr.a);
        WriteU16(out, instr.b);
        WriteU16(out, instr.c);
    }

    WriteU32(out, static_cast<uint32_t>(proto.upvalue_descs.size()));
    for (const auto& up : proto.upvalue_descs) {
        WriteU8(out, up.from_parent_local ? 1 : 0);
        WriteU16(out, up.index);
    }

    WriteU32(out, static_cast<uint32_t>(proto.child_protos.size()));
    for (const auto& child : proto.child_protos) {
        WriteProtoNode(*child, out, with_debug_info);
    }

    if (with_debug_info) {
        WriteU32(out, static_cast<uint32_t>(proto.debug_lines.size()));
        for (uint32_t line : proto.debug_lines) WriteU32(out, line);
        // debug_columns es nuevo (antes MakeFrameError solo tenia
        // linea); serializado siempre despues de debug_lines, tamano
        // propio por si algun Proto viejo en memoria no tiene una
        // entrada 1:1 con instructions (no deberia pasar, pero no vale
        // la pena asumirlo en el formato on-disk). Rompe compatibilidad
        // con .avapack generados antes de este cambio -- aceptable,
        // formato en desarrollo activo, no hay consumidores externos.
        WriteU32(out, static_cast<uint32_t>(proto.debug_columns.size()));
        for (uint32_t col : proto.debug_columns) WriteU32(out, col);
        WriteString(out, proto.debug_name);
        WriteString(out, proto.source_name);
    }
}

bool ReadProtoNode(avastd::istream& in, bool with_debug_info,
                    avastd::shared_ptr<Proto>& out_proto, avastd::string& error_out) {
    auto proto = avastd::make_shared<Proto>();

    uint16_t num_registers = 0;
    uint8_t num_params = 0, is_vararg = 0, is_method = 0;
    if (!ReadU16(in, num_registers) || !ReadU8(in, num_params) ||
        !ReadU8(in, is_vararg) || !ReadU8(in, is_method)) {
        error_out = "unexpected end of stream reading proto header";
        return false;
    }
    proto->num_registers = num_registers;
    proto->num_params = num_params;
    proto->is_vararg = is_vararg != 0;
    proto->is_method = is_method != 0;

    uint32_t const_count = 0;
    if (!ReadU32(in, const_count)) { error_out = "truncated constant count"; return false; }
    proto->constants.reserve(const_count);
    for (uint32_t i = 0; i < const_count; ++i) {
        Value v;
        if (!ReadValue(in, v)) { error_out = "corrupt constant entry"; return false; }
        proto->constants.push_back(v);
    }

    uint32_t instr_count = 0;
    if (!ReadU32(in, instr_count)) { error_out = "truncated instruction count"; return false; }
    proto->instructions.reserve(instr_count);
    for (uint32_t i = 0; i < instr_count; ++i) {
        uint8_t op = 0, a = 0;
        uint16_t b = 0, c = 0;
        if (!ReadU8(in, op) || !ReadU8(in, a) || !ReadU16(in, b) || !ReadU16(in, c)) {
            error_out = "truncated instruction";
            return false;
        }
        Instr instr;
        instr.op = static_cast<OpCode>(op);
        instr.a = a;
        instr.b = b;
        instr.c = c;
        proto->instructions.push_back(instr);
    }

    uint32_t upval_count = 0;
    if (!ReadU32(in, upval_count)) { error_out = "truncated upvalue count"; return false; }
    proto->upvalue_descs.reserve(upval_count);
    for (uint32_t i = 0; i < upval_count; ++i) {
        uint8_t from_parent_local = 0;
        uint16_t index = 0;
        if (!ReadU8(in, from_parent_local) || !ReadU16(in, index)) {
            error_out = "truncated upvalue descriptor";
            return false;
        }
        proto->upvalue_descs.push_back(UpvalDesc{from_parent_local != 0, index});
    }

    uint32_t child_count = 0;
    if (!ReadU32(in, child_count)) { error_out = "truncated child proto count"; return false; }
    proto->child_protos.reserve(child_count);
    for (uint32_t i = 0; i < child_count; ++i) {
        avastd::shared_ptr<Proto> child;
        if (!ReadProtoNode(in, with_debug_info, child, error_out)) return false;
        proto->child_protos.push_back(avastd::move(child));
    }

    if (with_debug_info) {
        uint32_t debug_line_count = 0;
        if (!ReadU32(in, debug_line_count)) { error_out = "truncated debug line count"; return false; }
        proto->debug_lines.reserve(debug_line_count);
        for (uint32_t i = 0; i < debug_line_count; ++i) {
            uint32_t line = 0;
            if (!ReadU32(in, line)) { error_out = "truncated debug line"; return false; }
            proto->debug_lines.push_back(line);
        }
        uint32_t debug_col_count = 0;
        if (!ReadU32(in, debug_col_count)) { error_out = "truncated debug column count"; return false; }
        proto->debug_columns.reserve(debug_col_count);
        for (uint32_t i = 0; i < debug_col_count; ++i) {
            uint32_t col = 0;
            if (!ReadU32(in, col)) { error_out = "truncated debug column"; return false; }
            proto->debug_columns.push_back(col);
        }
        if (!ReadString(in, proto->debug_name)) { error_out = "truncated debug_name"; return false; }
        if (!ReadString(in, proto->source_name)) { error_out = "truncated source_name"; return false; }
    }

    out_proto = avastd::move(proto);
    return true;
}

} // namespace

bool WriteProto(const Proto& root, avastd::ostream& out, const ProtoIoOptions& options) {
    out.write(kMagic, 4);
    WriteU16(out, kVersion);
    uint16_t flags = options.strip_debug_info ? 0 : kFlagHasDebugInfo;
    WriteU16(out, flags);
    WriteProtoNode(root, out, !options.strip_debug_info);
    return static_cast<bool>(out);
}

avastd::vector<uint8_t> SerializeProto(const Proto& root, const ProtoIoOptions& options) {
    avastd::ostringstream oss(avastd::ios::binary);
    WriteProto(root, oss, options);
    const avastd::string& s = oss.str();
    return avastd::vector<uint8_t>(s.begin(), s.end());
}

avastd::shared_ptr<Proto> ReadProto(avastd::istream& in, avastd::string* error_out) {
    char magic[4] = {0};
    if (!in.read(magic, 4) || avastd::memcmp(magic, kMagic, 4) != 0) {
        if (error_out) *error_out = "not an .avbc file (bad magic)";
        return nullptr;
    }
    uint16_t version = 0, flags = 0;
    if (!ReadU16(in, version) || !ReadU16(in, flags)) {
        if (error_out) *error_out = "truncated .avbc header";
        return nullptr;
    }
    if (version != kVersion) {
        if (error_out) *error_out = "unsupported .avbc version";
        return nullptr;
    }
    bool with_debug_info = (flags & kFlagHasDebugInfo) != 0;

    avastd::shared_ptr<Proto> root;
    avastd::string err;
    if (!ReadProtoNode(in, with_debug_info, root, err)) {
        if (error_out) *error_out = err;
        return nullptr;
    }
    return root;
}

avastd::shared_ptr<Proto> DeserializeProto(const avastd::vector<uint8_t>& bytes, avastd::string* error_out) {
    avastd::string s(bytes.begin(), bytes.end());
    avastd::istringstream iss(s, avastd::ios::binary);
    return ReadProto(iss, error_out);
}

} // namespace ava
