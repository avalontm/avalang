#include "BareKernelExternThunk.h"

#if !defined(__i386__)
#error "BareKernelExternThunk solo tiene implementacion i386 (cdecl)"
#endif

extern "C" avastd::int32_t ava_barekernel_thunk_call(void* fn, const void* arg_bytes, avastd::uint32_t arg_size);

namespace ava {
namespace platform {
namespace barekernel {

namespace {

void AppendBytes(avastd::vector<avastd::uint8_t>& buf, const void* data, avastd::size_t n) {
    avastd::size_t offset = buf.size();
    buf.resize(offset + n);
    avastd::memcpy(buf.data() + offset, data, n);
}

}

avastd::int32_t CallExternCdecl(void* fn, const avastd::vector<ExternArg>& args) {
    avastd::vector<avastd::uint8_t> buf;
    for (avastd::size_t i = 0; i < args.size(); ++i) {
        const ExternArg& a = args[i];
        switch (a.kind) {
            case ExternArgKind::Int32:   AppendBytes(buf, &a.i32, sizeof(a.i32)); break;
            case ExternArgKind::Int64:   AppendBytes(buf, &a.i64, sizeof(a.i64)); break;
            case ExternArgKind::Double:  AppendBytes(buf, &a.f64, sizeof(a.f64)); break;
            case ExternArgKind::Pointer: AppendBytes(buf, &a.ptr, sizeof(a.ptr)); break;
        }
    }
    return ava_barekernel_thunk_call(fn, buf.data(), static_cast<avastd::uint32_t>(buf.size()));
}

}
}
}

extern "C" __attribute__((naked)) avastd::int32_t
ava_barekernel_thunk_call(void*, const void*, avastd::uint32_t) {
    __asm__ volatile(
        "pushl %ebp\n"
        "movl %esp, %ebp\n"
        "pushl %esi\n"
        "pushl %edi\n"
        "pushl %ebx\n"
        "movl 8(%ebp), %ebx\n"
        "movl 12(%ebp), %esi\n"
        "movl 16(%ebp), %ecx\n"
        "subl %ecx, %esp\n"
        "movl %esp, %edi\n"
        "cld\n"
        "rep movsb\n"
        "call *%ebx\n"
        "leal -12(%ebp), %esp\n"
        "popl %ebx\n"
        "popl %edi\n"
        "popl %esi\n"
        "popl %ebp\n"
        "ret\n"
    );
}
