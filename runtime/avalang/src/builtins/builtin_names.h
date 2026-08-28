#ifndef AVA_BUILTINS_BUILTIN_NAMES_H
#define AVA_BUILTINS_BUILTIN_NAMES_H

// Fase 1 de PLAN_VALIDACION_ESTATICA.md.
//
// Unica fuente de verdad de los nombres de las funciones nativas "bare"
// -- las que se invocan como foo(...) (NameExpr), NO como obj.metodo()
// (esas son otra tabla: RegisterBuiltinMethod en builtin_registry.cpp,
// fuera del alcance de esta lista porque su receptor nunca es
// Type::Object y por eso Compiler::CheckMethodCallArgs ya las ignora sin
// necesidad de este chequeo).
//
// Antes de esta fase, esta lista estaba duplicada implicitamente: existia
// solo como una secuencia de llamadas a RegisterNative repartidas entre
// builtin_init.cpp (RegisterBuiltinGlobals) y builtin_registry.cpp
// (los ultimos 6 RegisterNative sueltos de RegisterBuiltinMethods, mal
// ubicados ahi por historia). El compiler no tenia forma de consultar
// ninguna de las dos, asi que Compiler::CheckCallArgs no podia distinguir
// "esto es un builtin" de "esto no existe".
//
// Con el X-macro de abajo hay un solo lugar donde agregar un native
// nuevo: X(nombre, funcion_c). Cada consumidor define X como necesite:
//   - builtin_init.cpp: X(name, fn) -> raw_vm->RegisterNative(#name, fn, ...)
//     (registra el nombre real en runtime, como antes).
//   - compiler.cpp: X(name, fn) -> #name (solo junta el string; ignora
//     fn, no necesita que el simbolo builtin_fn este ni declarado).
// Los dos lados quedan sincronizados por construccion en vez de por
// convencion -- agregar un native y olvidarse de un lado ya no es
// posible sin que el otro lado tambien lo vea.
//
// IMPORTANTE: esta lista es de solo nombres. Los identificadores del
// segundo argumento de cada X(...) (builtin_type, builtin_str, etc.) son
// tokens de macro, no se resuelven aca -- cada .cpp que expanda esta
// lista usando el segundo argumento debe incluir el header que declara
// esas funciones (builtin_natives.h y/o builtin.h). El compiler no las
// usa, asi que compiler.cpp puede incluir este header sin arrastrar
// ninguna dependencia de la VM.
#define AVA_BUILTIN_GLOBALS(X) \
    X(type, builtin_type) \
    X(str, builtin_str) \
    X(int, builtin_int) \
    X(float, builtin_float) \
    X(print, builtin_print) \
    X(input, builtin_input) \
    X(abs, builtin_abs) \
    X(round, builtin_round) \
    X(floor, builtin_floor) \
    X(ceil, builtin_ceil) \
    X(min, builtin_min) \
    X(max, builtin_max) \
    X(pow, builtin_pow) \
    X(sqrt, builtin_sqrt) \
    X(sum, builtin_sum) \
    X(sorted, builtin_sorted) \
    X(reversed, builtin_reversed) \
    X(any, builtin_any) \
    X(all, builtin_all) \
    X(len, builtin_len) \
    X(range, builtin_range) \
    X(slice, builtin_slice) \
    X(setglobal, builtin_setglobal) \
    /* Requerido por todo `import` -- ver builtin_import en */ \
    /* builtin_natives.h para el porque tiene que registrarse aca. */ \
    X(__import__, builtin_import) \
    /* Memoria cruda para decodificar retornos de `extern` que en C son */ \
    /* char-pointer o char-pointer-pointer (ver builtin_mem.cpp), p.ej. */ \
    /* mysql_error(). */ \
    X(mem_is_null, builtin_mem_is_null) \
    X(mem_peek_string, builtin_mem_peek_string) \
    X(mem_peek_ptr, builtin_mem_peek_ptr) \
    /* Antes registrados sueltos en builtin_registry.cpp; movidos aca en */ \
    /* la Fase 1 porque tambien son bare natives (coroutine(...), */ \
    /* resume(...), etc.), no metodos dotted -- pertenecian a esta lista. */ \
    X(coroutine, builtin_coroutine) \
    X(resume, builtin_resume) \
    X(set_timeout, builtin_set_timeout) \
    X(sleep_async, builtin_sleep_async) \
    X(clear_timeout, builtin_clear_timeout) \
    X(delay, builtin_delay)

#endif // AVA_BUILTINS_BUILTIN_NAMES_H
