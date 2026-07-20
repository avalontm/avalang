#include <cstdio>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstring>
#include "ava.h"
#include "../vm/vm.h"
#include "../vm/value.h"
#include "../builtins/builtin.h"

#ifndef NDEBUG
#define DEBUG_PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG_PRINT(...) ((void)0)
#endif

static ava_value_t native_print(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    (void)vm;
    for (size_t i = 0; i < count; i++) {
        if (i > 0) std::printf(" ");
        switch (args[i].type) {
            case AVA_NIL: std::printf("nil"); break;
            case AVA_BOOL: std::printf("%s", args[i].as.b ? "true" : "false"); break;
            case AVA_NUMBER: std::printf("%g", args[i].as.n); break;
            case AVA_STRING: {
                size_t len = 0;
                const char* s = ava_string_data(vm, args[i], &len);
                std::printf("%.*s", (int)len, s);
                break;
            }
            case AVA_LIST: {
                size_t len = ava_list_length(vm, args[i]);
                std::printf("[");
                for (size_t j = 0; j < len; j++) {
                    if (j > 0) std::printf(", ");
                    ava_value_t item = ava_list_get(vm, args[i], j);
                    switch (item.type) {
                        case AVA_NIL: std::printf("nil"); break;
                        case AVA_BOOL: std::printf("%s", item.as.b ? "true" : "false"); break;
                        case AVA_NUMBER: std::printf("%g", item.as.n); break;
                        case AVA_STRING: {
                            size_t slen = 0;
                            const char* ss = ava_string_data(vm, item, &slen);
                            std::printf("\"%.*s\"", (int)slen, ss);
                            break;
                        }
                        default: std::printf("<object>"); break;
                    }
                }
                std::printf("]");
                break;
            }
            default: std::printf("<object>"); break;
        }
    }
    std::printf("\n");
    ava_value_t nil{}; nil.type = AVA_NIL;
    return nil;
}

static ava_value_t native_len(AvaVM*, const ava_value_t* args, size_t count, void*) {
    ava_value_t nil{}; nil.type = AVA_NIL;
    if (count < 1) return nil;
    if (args[0].type == AVA_LIST) {
        ava_value_t r{}; r.type = AVA_NUMBER; r.as.n = ava_list_length(nullptr, args[0]);
        return r;
    }
    if (args[0].type == AVA_STRING) {
        size_t len = 0; ava_string_data(nullptr, args[0], &len);
        ava_value_t r{}; r.type = AVA_NUMBER; r.as.n = len;
        return r;
    }
    if (args[0].type == AVA_DICT) {
        ava_value_t r{}; r.type = AVA_NUMBER; r.as.n = ava_dict_length(nullptr, args[0]);
        return r;
    }
    return nil;
}

static ava_value_t native_range(AvaVM*, const ava_value_t* args, size_t count, void*) {
    double start = 0, end = 0, step = 1;
    if (count == 1) {
        end = args[0].as.n;
    } else if (count >= 2) {
        start = args[0].as.n;
        end = args[1].as.n;
        if (count >= 3) step = args[2].as.n;
    }
    if (step == 0) step = 1;
    ava_value_t list = ava_list_create(nullptr);
    double val = start;
    if (step > 0) {
        while (val < end) {
            ava_value_t n{}; n.type = AVA_NUMBER; n.as.n = val;
            ava_list_append(nullptr, list, n);
            val += step;
        }
    } else {
        while (val > end) {
            ava_value_t n{}; n.type = AVA_NUMBER; n.as.n = val;
            ava_list_append(nullptr, list, n);
            val += step;
        }
    }
    return list;
}

static ava_value_t make_nil() {
    ava_value_t v{}; v.type = AVA_NIL; return v;
}

static ava_value_t make_number(double n) {
    ava_value_t v{}; v.type = AVA_NUMBER; v.as.n = n; return v;
}

static ava_value_t make_string(const std::string& s) {
    ava_value_t v = ava_list_create(nullptr);
    ava_string_free(nullptr);
    return v;
}

static ava_value_t native_type(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return make_nil();
    const char* names[] = {
        "nil", "bool", "number", "string", "list", "dict",
        "function", "instance", "class", "coroutine",
        "native", "bound", "exception"
    };
    int idx = static_cast<int>(args[0].type);
    if (idx < 0 || idx > 12) idx = 0;
    fprintf(stderr, "[C++] native_type: arg0.type=%d (%s)\n", (int)args[0].type, names[idx]);
    ava_value_t str = ava_string_create(nullptr, names[idx], strlen(names[idx]));
    fprintf(stderr, "[C++] native_type: returning type=%d, ref.id=%lu\n", (int)str.type, (unsigned long)str.as.ref.id);
    return str;
}

static std::string value_to_string(AvaVM* vm, const ava_value_t& v) {
    switch (v.type) {
        case AVA_NIL: return "nil";
        case AVA_BOOL: return v.as.b ? "true" : "false";
        case AVA_NUMBER: {
            double n = v.as.n;
            if (n == std::floor(n) && std::abs(n) < 1e15) {
                return std::to_string(static_cast<int64_t>(n));
            }
            return std::to_string(n);
        }
        case AVA_STRING: {
            size_t len = 0;
            const char* s = ava_string_data(vm, v, &len);
            return std::string(s, len);
        }
        default: return "<object>";
    }
}

static ava_value_t native_str(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return make_nil();
    std::string s = value_to_string(vm, args[0]);
    ava_value_t result = ava_string_create(vm, s.c_str(), s.size());
    return result;
}

static ava_value_t native_int(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return make_nil();
    if (args[0].type == AVA_NUMBER) {
        return make_number(static_cast<int64_t>(args[0].as.n));
    }
    if (args[0].type == AVA_STRING) {
        size_t len = 0;
        const char* s = ava_string_data(nullptr, args[0], &len);
        try {
            return make_number(std::stoll(std::string(s, len)));
        } catch (...) {
            return make_nil();
        }
    }
    return make_nil();
}

static ava_value_t native_float(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return make_nil();
    if (args[0].type == AVA_NUMBER) {
        return make_number(args[0].as.n);
    }
    if (args[0].type == AVA_STRING) {
        size_t len = 0;
        const char* s = ava_string_data(nullptr, args[0], &len);
        try {
            return make_number(std::stod(std::string(s, len)));
        } catch (...) {
            return make_nil();
        }
    }
    return make_nil();
}

static ava_value_t native_abs(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return make_nil();
    if (args[0].type == AVA_NUMBER) {
        return make_number(std::abs(args[0].as.n));
    }
    return make_nil();
}

static ava_value_t native_round(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return make_nil();
    if (args[0].type == AVA_NUMBER) {
        return make_number(std::round(args[0].as.n));
    }
    return make_nil();
}

static ava_value_t native_floor(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return make_nil();
    if (args[0].type == AVA_NUMBER) {
        return make_number(std::floor(args[0].as.n));
    }
    return make_nil();
}

static ava_value_t native_ceil(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return make_nil();
    if (args[0].type == AVA_NUMBER) {
        return make_number(std::ceil(args[0].as.n));
    }
    return make_nil();
}

static ava_value_t native_min(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return make_nil();
    double min_val = 0;
    bool found = false;
    for (size_t i = 0; i < count; i++) {
        if (args[i].type == AVA_NUMBER) {
            if (!found || args[i].as.n < min_val) {
                min_val = args[i].as.n;
                found = true;
            }
        }
    }
    return found ? make_number(min_val) : make_nil();
}

static ava_value_t native_max(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return make_nil();
    double max_val = 0;
    bool found = false;
    for (size_t i = 0; i < count; i++) {
        if (args[i].type == AVA_NUMBER) {
            if (!found || args[i].as.n > max_val) {
                max_val = args[i].as.n;
                found = true;
            }
        }
    }
    return found ? make_number(max_val) : make_nil();
}

static ava_value_t native_pow(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 2) return make_nil();
    if (args[0].type == AVA_NUMBER && args[1].type == AVA_NUMBER) {
        return make_number(std::pow(args[0].as.n, args[1].as.n));
    }
    return make_nil();
}

static ava_value_t native_sqrt(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return make_nil();
    if (args[0].type == AVA_NUMBER) {
        return make_number(std::sqrt(args[0].as.n));
    }
    return make_nil();
}

static ava_value_t native_sum(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return make_nil();
    if (args[0].type == AVA_LIST) {
        double total = 0;
        size_t len = ava_list_length(nullptr, args[0]);
        for (size_t i = 0; i < len; i++) {
            ava_value_t item = ava_list_get(nullptr, args[0], i);
            if (item.type == AVA_NUMBER) {
                total += item.as.n;
            }
        }
        return make_number(total);
    }
    return make_nil();
}

static ava_value_t native_any(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return make_nil();
    if (args[0].type == AVA_LIST) {
        size_t len = ava_list_length(nullptr, args[0]);
        for (size_t i = 0; i < len; i++) {
            ava_value_t item = ava_list_get(nullptr, args[0], i);
            if (item.type == AVA_BOOL) {
                if (item.as.b) { ava_value_t r{}; r.type = AVA_BOOL; r.as.b = 1; return r; }
            } else if (item.type != AVA_NIL) {
                ava_value_t r{}; r.type = AVA_BOOL; r.as.b = 1; return r;
            }
        }
        ava_value_t r{}; r.type = AVA_BOOL; r.as.b = 0; return r;
    }
    return make_nil();
}

static ava_value_t native_all(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return make_nil();
    if (args[0].type == AVA_LIST) {
        size_t len = ava_list_length(nullptr, args[0]);
        for (size_t i = 0; i < len; i++) {
            ava_value_t item = ava_list_get(nullptr, args[0], i);
            if (item.type == AVA_BOOL) {
                if (!item.as.b) { ava_value_t r{}; r.type = AVA_BOOL; r.as.b = 0; return r; }
            } else if (item.type == AVA_NIL) {
                ava_value_t r{}; r.type = AVA_BOOL; r.as.b = 0; return r;
            }
        }
        ava_value_t r{}; r.type = AVA_BOOL; r.as.b = 1; return r;
    }
    return make_nil();
}

static ava_value_t native_sorted(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return make_nil();
    if (args[0].type == AVA_LIST) {
        std::vector<double> numbers;
        size_t len = ava_list_length(nullptr, args[0]);
        for (size_t i = 0; i < len; i++) {
            ava_value_t item = ava_list_get(nullptr, args[0], i);
            if (item.type == AVA_NUMBER) {
                numbers.push_back(item.as.n);
            }
        }
        std::sort(numbers.begin(), numbers.end());
        ava_value_t result = ava_list_create(nullptr);
        for (double n : numbers) {
            ava_value_t v{}; v.type = AVA_NUMBER; v.as.n = n;
            ava_list_append(nullptr, result, v);
        }
        return result;
    }
    return make_nil();
}

static ava_value_t native_reversed(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return make_nil();
    if (args[0].type == AVA_LIST) {
        size_t len = ava_list_length(nullptr, args[0]);
        ava_value_t result = ava_list_create(nullptr);
        for (size_t i = len; i > 0; i--) {
            ava_value_t item = ava_list_get(nullptr, args[0], i - 1);
            ava_list_append(nullptr, result, item);
        }
        return result;
    }
    return make_nil();
}

static ava_value_t native_slice(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return make_nil();
    
    double start_val = 0;
    double end_val = 0;
    double step_val = 1;
    bool has_end = false;
    
    // NOTE: args[0] is the list itself, args[1]=start, args[2]=end,
    // args[3]=step. The bounds check for reading args[i] is count > i,
    // i.e. count >= i+1 -- previously these were off by one (count>=3/4/5
    // instead of >=2/3/4), which meant step (args[3]) required count>=5
    // but the slice() call site always passes exactly 4 args, so step was
    // silently never read.
    if (count >= 2) {
        if (args[1].type == AVA_NUMBER) {
            start_val = args[1].as.n;
        }
    }
    if (count >= 3) {
        if (args[2].type == AVA_NUMBER) {
            end_val = args[2].as.n;
            has_end = true;
        }
    }
    if (count >= 4) {
        if (args[3].type == AVA_NUMBER) {
            step_val = args[3].as.n;
            if (step_val == 0) step_val = 1;
        }
    }
    
    if (args[0].type == AVA_LIST) {
        size_t len = ava_list_length(nullptr, args[0]);
        
        size_t start_idx;
        size_t end_idx;
        int step = static_cast<int>(step_val);
        
        if (step > 0) {
            if (start_val < 0) {
                start_idx = (size_t)std::max(0, (int)len + (int)start_val);
            } else {
                start_idx = (size_t)std::min((size_t)start_val, len);
            }
            
            if (!has_end) {
                // No end argument given at all (nil) -- slice through the end.
                end_idx = len;
            } else if (end_val < 0) {
                end_idx = (size_t)std::max(0, (int)len + (int)end_val);
            } else {
                end_idx = (size_t)std::min((size_t)end_val, len);
            }
            
            ava_value_t result = ava_list_create(nullptr);
            for (size_t i = start_idx; i < end_idx && i < len; i += step) {
                ava_value_t item = ava_list_get(nullptr, args[0], i);
                ava_list_append(nullptr, result, item);
            }
            return result;
        } else {
            if (start_val < 0) {
                start_idx = (size_t)std::max(0, (int)len + (int)start_val);
            } else {
                start_idx = (size_t)std::min((size_t)start_val, len);
            }
            
            if (end_val < 0) {
                end_idx = (size_t)std::max(0, (int)len + (int)end_val);
            } else {
                end_idx = (size_t)std::min((size_t)end_val, len);
            }
            
            if (end_idx == 0) end_idx = 0;
            
            ava_value_t result = ava_list_create(nullptr);
            if (len > 0 && start_idx < len && start_idx >= end_idx) {
                for (size_t i = start_idx; ; i -= step) {
                    if (i < len) {
                        ava_value_t item = ava_list_get(nullptr, args[0], i);
                        ava_list_append(nullptr, result, item);
                    }
                    if (i == 0 || i <= end_idx) break;
                    if (i < (size_t)(-step)) break;
                }
            }
            return result;
        }
    }
    
    if (args[0].type == AVA_STRING) {
        size_t str_len = 0;
        ava_string_data(nullptr, args[0], &str_len);
        
        size_t start_idx;
        size_t end_idx;
        int step = static_cast<int>(step_val);
        
        if (step > 0) {
            if (start_val < 0) {
                start_idx = (size_t)std::max(0, (int)str_len + (int)start_val);
            } else {
                start_idx = (size_t)std::min((size_t)start_val, str_len);
            }
            
            if (!has_end) {
                end_idx = str_len;
            } else if (end_val < 0) {
                end_idx = (size_t)std::max(0, (int)str_len + (int)end_val);
            } else {
                end_idx = (size_t)std::min((size_t)end_val, str_len);
            }
            
            std::string result;
            const char* s = ava_string_data(nullptr, args[0], nullptr);
            for (size_t i = start_idx; i < end_idx && i < str_len; i += step) {
                result += s[i];
            }
            return ava_string_create(nullptr, result.c_str(), result.size());
        } else {
            if (start_val < 0) {
                start_idx = (size_t)std::max(0, (int)str_len + (int)start_val);
            } else {
                start_idx = (size_t)std::min((size_t)start_val, str_len);
            }
            
            if (end_val < 0) {
                end_idx = (size_t)std::max(0, (int)str_len + (int)end_val);
            } else {
                end_idx = (size_t)std::min((size_t)end_val, str_len);
            }
            
            if (end_idx == 0) end_idx = 0;
            
            std::string result;
            const char* s = ava_string_data(nullptr, args[0], nullptr);
            if (str_len > 0 && start_idx < str_len && start_idx >= end_idx) {
                for (size_t i = start_idx; ; i -= step) {
                    if (i < str_len) {
                        result += s[i];
                    }
                    if (i == 0 || i <= end_idx) break;
                    if (i < (size_t)(-step)) break;
                }
            }
            return ava_string_create(nullptr, result.c_str(), result.size());
        }
    }
    
    return make_nil();
}

static ava_value_t native_import(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (count < 1 || args[0].type != AVA_STRING) return make_nil();
    size_t path_len = 0;
    const char* path = ava_string_data(vm, args[0], &path_len);
    const char* alias = nullptr;
    if (count >= 2 && args[1].type == AVA_STRING) {
        size_t alias_len = 0;
        alias = ava_string_data(vm, args[1], &alias_len);
    }
    char* error = nullptr;
    ava_value_t result = ava_import(vm, path, alias, &error);
    if (error) {
        std::fprintf(stderr, "import error: %s\n", error);
        ava_string_free(error);
        return make_nil();
    }
    
    return result;
}

static ava_value_t native_raise(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    ava_value_t nil{}; nil.type = AVA_NIL;
    if (count < 1) return nil;
    
    std::string message;
    
    if (args[0].type == AVA_STRING) {
        size_t len = 0;
        const char* msg = ava_string_data(vm, args[0], &len);
        message = std::string(msg, len);
    } else if (args[0].type == AVA_DICT) {
        ava_value_t msg_val = ava_dict_get(vm, args[0], "message");
        if (msg_val.type == AVA_STRING) {
            size_t len = 0;
            const char* m = ava_string_data(vm, msg_val, &len);
            message = std::string(m, len);
        }
    }
    
    ava_value_t exc_str = ava_string_create(vm, message.c_str(), message.size());
    ava_value_t exc{};
    exc.type = AVA_EXCEPTION;
    exc.as.err = exc_str.as.ref;
    
    ava::VM* raw_vm = reinterpret_cast<ava::VM*>(vm);
    raw_vm->RaiseException(ava::FromC(exc));
    
    return nil;
}

int main(int argc, char** argv) {
    DEBUG_PRINT("DEBUG: ava_cli started, argc=%d\n", argc);
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <script.ava>\n", argv[0]);
        return 1;
    }
    DEBUG_PRINT("DEBUG: opening file %s\n", argv[1]);

    std::ifstream file(argv[1]);
    if (!file) {
        std::fprintf(stderr, "error: could not open %s\n", argv[1]);
        return 1;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();

    AvaVM* vm = ava_vm_create();
    
    RegisterBuiltinMethods(vm);
    
    {
        std::string script_dir = argv[1];
        size_t sep = script_dir.find_last_of("/\\");
        if (sep != std::string::npos) {
            script_dir = script_dir.substr(0, sep);
            ava::VM* raw_vm = reinterpret_cast<ava::VM*>(vm);
            raw_vm->GetModuleResolver().AddSearchPath(script_dir);
        }
    }
    
    ava_vm_register_native(vm, "print", native_print, nullptr);
    ava_vm_register_native(vm, "len", native_len, nullptr);
    ava_vm_register_native(vm, "range", native_range, nullptr);
    ava_vm_register_native(vm, "type", native_type, nullptr);
    ava_vm_register_native(vm, "str", native_str, nullptr);
    ava_vm_register_native(vm, "int", native_int, nullptr);
    ava_vm_register_native(vm, "float", native_float, nullptr);
    ava_vm_register_native(vm, "abs", native_abs, nullptr);
    ava_vm_register_native(vm, "round", native_round, nullptr);
    ava_vm_register_native(vm, "floor", native_floor, nullptr);
    ava_vm_register_native(vm, "ceil", native_ceil, nullptr);
    ava_vm_register_native(vm, "min", native_min, nullptr);
    ava_vm_register_native(vm, "max", native_max, nullptr);
    ava_vm_register_native(vm, "pow", native_pow, nullptr);
    ava_vm_register_native(vm, "sqrt", native_sqrt, nullptr);
    ava_vm_register_native(vm, "sum", native_sum, nullptr);
    ava_vm_register_native(vm, "any", native_any, nullptr);
    ava_vm_register_native(vm, "all", native_all, nullptr);
    ava_vm_register_native(vm, "sorted", native_sorted, nullptr);
    ava_vm_register_native(vm, "reversed", native_reversed, nullptr);
    ava_vm_register_native(vm, "slice", native_slice, nullptr);
    ava_vm_register_native(vm, "__import__", native_import, nullptr);
    ava_vm_register_native(vm, "raise", native_raise, nullptr);

    char* error = nullptr;
    DEBUG_PRINT("DEBUG: calling ava_compile\n");
    AvaModule* module = ava_compile(vm, buffer.str().c_str(), argv[1], &error);
    DEBUG_PRINT("DEBUG: ava_compile returned, module=%p, error=%p\n", module, error);
    if (!module) {
        std::fprintf(stderr, "compile error: %s\n", error ? error : "unknown error");
        if (error) ava_string_free(error);
        ava_vm_destroy(vm);
        return 1;
    }

    DEBUG_PRINT("DEBUG: calling ava_run\n");
    ava_value_t result{};
    ava_run(vm, module, &result, &error);
    DEBUG_PRINT("DEBUG: ava_run returned, error=%p, result.type=%d, result.n=%g\n", error, (int)result.type, result.as.n);
    if (error) {
        std::fprintf(stderr, "runtime error: %s\n", error);
        ava_string_free(error);
        ava_module_destroy(module);
        ava_vm_destroy(vm);
        return 1;
    }

    ava_module_destroy(module);
    ava_vm_destroy(vm);
    return 0;
}