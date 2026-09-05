#include "vm.h"
#include "vm_internal.h"


namespace ava {

void OpNewClass(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    auto* cls = new ClassObj();
    auto* class_proto = static_cast<ClassObj*>(K[in.b].obj);
    cls->name = class_proto->name;
    cls->attrs = class_proto->attrs;
    cls->instance_defaults = class_proto->instance_defaults;
    cls->methods = class_proto->methods;
    cls->private_members = class_proto->private_members;
    cls->param_names = class_proto->param_names;
    // v recien creado: ref_count=1 propio; la asignacion de abajo ya
    // Retiene (RAII). El Retain(v) manual dejaba una referencia de mas.
    Value v; v.type = ValueType::Class; v.obj = cls;
    frame.registers[in.a] = v;
}

void OpNewInstance(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    auto& cls_val = frame.registers[in.b];
    auto* cls = static_cast<ClassObj*>(cls_val.obj);
    auto* inst = new InstanceObj();
    inst->cls = cls;
    inst->attrs = cls->instance_defaults;
    
    // Bug #13: `cls` viene de `cls_val` (objeto YA existente en el
    // registro), no de un `new` recien hecho -- copiar `cls_val` retiene
    // correctamente en vez de armar una Value manual que Release() de mas
    // al salir de scope (mismo patron que vm.cpp/vm_call_op.cpp).
    Value cls_val_copy = cls_val;
    inst->attrs["__class__"] = cls_val_copy;
    
    auto base_it = cls->attrs.find("__base__");
    if (base_it != cls->attrs.end()) {
        inst->attrs["__base__"] = base_it->second;
    }
    Value v; v.type = ValueType::Instance; v.obj = inst;
    frame.registers[in.a] = v;
}

void OpGetAttr(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    auto& obj = frame.registers[in.b];
    auto* attr_name = static_cast<StringObj*>(K[in.c].obj);
    
    if (obj.type == ValueType::String || obj.type == ValueType::List || obj.type == ValueType::Dict) {
        avastd::string method_name = attr_name->data;
        
        avastd::string prefixed_name;
        if (obj.type == ValueType::String) {
            prefixed_name = "str_" + method_name;
        } else if (obj.type == ValueType::List) {
            prefixed_name = "list_" + method_name;
        } else if (obj.type == ValueType::Dict) {
            prefixed_name = "dict_" + method_name;
        }
        
        if (vm.HasBuiltinMethod(prefixed_name)) {
            auto* native = new NativeObj();
            native->fn = vm.builtin_methods_[prefixed_name].first;
            native->user_data = vm.builtin_methods_[prefixed_name].second;
            native->is_primitive_method = true;
            native->primitive_this = ToC(obj);
            // Retiene lo que ToC() no retiene por su cuenta (ToC es solo
            // un cast de Value a ava_value_t, ver value.cpp) -- este
            // NativeObj ahora es dueño real de una referencia a `obj`
            // mientras viva. ~NativeObj() (value.cpp) suelta esto.
            Retain(obj);
            Value v; v.type = ValueType::Native; v.obj = native;
            frame.registers[in.a] = v;
            return;
        }
    }
    
    if (obj.type == ValueType::Dict) {
        auto* dict = static_cast<DictObj*>(obj.obj);
        auto it = dict->index.find(attr_name->data);
        if (it != dict->index.end()) {
            frame.registers[in.a] = dict->entries[it->second].second;
        } else {
            frame.registers[in.a] = Value::Nil();
        }
    } else if (obj.type == ValueType::Instance) {
        auto* inst = static_cast<InstanceObj*>(obj.obj);
        
        auto class_it = inst->attrs.find("__class__");
        ClassObj* lookup_cls = inst->cls;
        if (class_it != inst->attrs.end() && class_it->second.type == ValueType::Class) {
            lookup_cls = static_cast<ClassObj*>(class_it->second.obj);
        }
        
        auto it = inst->attrs.find(attr_name->data);
        if (it != inst->attrs.end()) {
            frame.registers[in.a] = it->second;
        } else {
            auto method_it = lookup_cls->methods.find(attr_name->data);
            if (method_it != lookup_cls->methods.end()) {
                auto* bound = new BoundMethod();
                bound->proto = method_it->second;
                // bound->instance = obj ya Retiene via copy-assignment
                // (RAII); el Retain(obj) manual (sobre el mismo objeto,
                // via otra Value que lo referencia) y el Retain(v) de
                // abajo eran retenciones de mas, no balanceadas por
                // ningun Release.
                bound->instance = obj;
                Value v; v.type = ValueType::Bound; v.obj = bound;
                frame.registers[in.a] = v;
            } else {
                // Fallback a atributos `static` de clase (viven en
                // cls->attrs, no en cls->instance_defaults, así que nunca
                // se copian a inst->attrs -- ver value.h). Sin esto,
                // `this.total` dentro de un método nunca encontraba un
                // `static total = ...` declarado en la clase y devolvía
                // Nil incluso cuando `Contador.total` sí funcionaba.
                // NOTA (corregida, ver bug #15 en BUGS_ENCONTRADOS.md):
                // una nota anterior de esta misma línea decía que había un
                // bug "distinto y aún abierto" con `this.total` DENTRO de
                // un método -- primero se afirmó que GETATTR nunca se
                // emitía para ese caso, y en una revisión posterior que el
                // problema real era que el RETURN que sigue descartaba ese
                // resultado. Ninguna de las dos resultó cierta: `this.total`
                // (este mismo fallback) funciona correctamente cuando se
                // accede DIRECTAMENTE dentro del cuerpo de un método -- no
                // se pudo reproducir ningún caso donde fallara. El bug real
                // (bug #15, ✅ corregido) era otro y más acotado: solo
                // ocurría con `this.attr`/`this.metodo()`/`base.metodo()`
                // dentro de un lambda o `func` anidado DEFINIDO DENTRO de
                // un método (no en el método mismo) -- "this" se excluía a
                // propósito de la captura de upvalues del compilador para
                // esos casos. El fix está en compiler.cpp (`CompileExpr`,
                // casos `LambdaExpr`/`BaseExpr`, y `CompileFunctionDecl`),
                // no acá -- este código (el fallback de atributos static)
                // nunca tuvo el bug.
                auto* owner = FindClassOwningAttr(lookup_cls, attr_name->data);
                if (owner) {
                    frame.registers[in.a] = owner->attrs.at(attr_name->data);
                } else {
                    frame.registers[in.a] = Value::Nil();
                }
            }
        }
    } else if (obj.type == ValueType::Class) {
        auto* cls = static_cast<ClassObj*>(obj.obj);
        auto method_it = cls->methods.find(attr_name->data);
        if (method_it != cls->methods.end()) {
            auto* bound = new BoundMethod();
            bound->proto = method_it->second;
            bound->instance = Value::Nil();
            Value v; v.type = ValueType::Bound; v.obj = bound;
            frame.registers[in.a] = v;
        } else {
            auto* owner = FindClassOwningAttr(cls, attr_name->data);
            if (owner) {
                frame.registers[in.a] = owner->attrs.at(attr_name->data);
            } else {
                frame.registers[in.a] = Value::Nil();
            }
        }
    } else if (obj.type == ValueType::Module) {
        // Namespace de un bloque `extern`.
        // No hay herencia ni "this": es solo un mapa nombre -> Native.
        auto* mod = static_cast<ModuleObj*>(obj.obj);
        auto it = mod->attrs.find(attr_name->data);
        if (it != mod->attrs.end()) {
            frame.registers[in.a] = it->second;
        } else {
            frame.registers[in.a] = Value::Nil();
        }
    } else {
        frame.registers[in.a] = Value::Nil();
    }
}

void OpSetAttr(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    auto& obj = frame.registers[in.a];
    auto* attr_name = static_cast<StringObj*>(K[in.b].obj);
    auto& val = frame.registers[in.c];
    // `val` es una referencia a un registro existente (ya posee su
    // propia referencia retenida). El operator= de Value (RAII,
    // sub-fase 2) ya Retiene val y Libera el valor viejo por su cuenta
    // en cada asignacion de abajo -- los Release()/Retain() manuales que
    // rodeaban estas asignaciones duplicaban ambos lados (double-release
    // real sobre el valor viejo, mas un Retain de mas sobre val).
    if (obj.type == ValueType::Instance) {
        auto* inst = static_cast<InstanceObj*>(obj.obj);
        auto it = inst->attrs.find(attr_name->data);
        if (it != inst->attrs.end()) {
            it->second = val;
        } else {
            // Antes de crear una copia de instancia, chequear si este
            // nombre es un `static` declarado en la clase (vive en
            // cls->attrs, walk por __base__ vía FindClassOwningAttr).
            // Sin este chequeo, `this.total = ...` sobre un atributo
            // estático nunca escrito antes en esta instancia creaba un
            // `total` de instancia nuevo en vez de mutar el compartido de
            // clase, así que dos instancias (o dos llamadas que leían via
            // `Contador.total`) dejaban de ver el mismo valor.
            auto inst_class_it = inst->attrs.find("__class__");
            ClassObj* lookup_cls = inst->cls;
            if (inst_class_it != inst->attrs.end() && inst_class_it->second.type == ValueType::Class) {
                lookup_cls = static_cast<ClassObj*>(inst_class_it->second.obj);
            }
            auto* owner = lookup_cls ? FindClassOwningAttr(lookup_cls, attr_name->data) : nullptr;
            if (owner) {
                auto owner_it = owner->attrs.find(attr_name->data);
                owner_it->second = val;
            } else {
                inst->attrs[attr_name->data] = val;
            }
        }
    } else if (obj.type == ValueType::Class) {
        auto* cls = static_cast<ClassObj*>(obj.obj);
        auto* owner = FindClassOwningAttr(cls, attr_name->data);
        auto& target_attrs = owner ? owner->attrs : cls->attrs;
        auto it = target_attrs.find(attr_name->data);
        if (it != target_attrs.end()) {
            it->second = val;
        } else {
            target_attrs[attr_name->data] = val;
        }
    } else if (obj.type == ValueType::Dict) {
        auto* dict = static_cast<DictObj*>(obj.obj);
        auto it = dict->index.find(attr_name->data);
        if (it != dict->index.end()) {
            dict->entries[it->second].second = val;
        } else {
            dict->index[attr_name->data] = dict->entries.size();
            // emplace_back copy-construye el Value -> ya Retiene (RAII).
            dict->entries.emplace_back(attr_name->data, val);
        }
    }
}

} // namespace ava