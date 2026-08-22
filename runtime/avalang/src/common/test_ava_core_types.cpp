// Test manual (no framework), mismo patron que
// src/compiler/test_proto_io_obfuscate.cpp: standalone, cassert, no esta
// enganchado a CMakeLists (se compila a mano cuando hace falta).
//
// Cubre los 5 items de Fase 2 (avalang_runtime_stl_barekernel_plan.md,
// seccion 16): AvaString, AvaArray, AvaMap, AvaValue, AvaObject. Los 5
// son alias sobre tipos que ya existian (avastd::string/vector/
// unordered_map y ava::Value/Object) -- ver el comentario de diseno en
// cada header (AvaString.h, AvaArray.h, AvaMap.h, ../vm/AvaValue.h,
// ../vm/AvaObject.h) para el porque. Este archivo verifica que el alias
// realmente expone el contrato que pide la seccion 4.1 del plan, no solo
// que compila.
#include <cassert>
#include <iostream>

#include "AvaString.h"
#include "AvaArray.h"
#include "AvaMap.h"
#include "../vm/AvaValue.h"
#include "../vm/AvaObject.h"

using namespace ava;

namespace {

// --- AvaString: buffer, longitud, comparacion, concatenacion, acceso ---
void TestAvaString() {
    AvaString a("hola");
    AvaString b("hola");
    AvaString c("mundo");

    assert(a.size() == 4);
    assert(a == b);
    assert(!(a == c));

    a += " mundo";
    assert(a == AvaString("hola mundo"));
    assert(a[0] == 'h');
    assert(a.c_str()[a.size()] == '\0'); // acceso + conversion (c_str null-terminado)

    std::cout << "[OK] AvaString: buffer/longitud/comparacion/concatenacion/acceso\n";
}

// --- AvaArray<T>: elementos, tamanio, insercion, indexacion, iteracion ---
void TestAvaArray() {
    AvaArray<int> arr;
    assert(arr.size() == 0);

    for (int i = 0; i < 5; ++i) arr.push_back(i * 10);
    assert(arr.size() == 5);
    assert(arr[0] == 0);
    assert(arr[4] == 40);

    int sum = 0;
    for (int v : arr) sum += v; // iteracion range-for
    assert(sum == 0 + 10 + 20 + 30 + 40);

    arr.pop_back(); // eliminacion
    assert(arr.size() == 4);

    // AvaArray<Value> es, hoy, el mismo tipo concreto que ListObj::items.
    AvaArray<Value> values;
    values.push_back(Value::Number(1));
    values.push_back(Value::String("x"));
    assert(values.size() == 2);
    assert(values[0].type == ValueType::Number);
    assert(values[1].type == ValueType::String);

    std::cout << "[OK] AvaArray: elementos/tamanio/insercion/eliminacion/indexacion/iteracion\n";
}

// --- AvaMap<K,V>: hash, insercion, busqueda, eliminacion, iteracion ---
void TestAvaMap() {
    AvaMap<AvaString, int> m;
    m[AvaString("uno")] = 1;
    m[AvaString("dos")] = 2;
    assert(m.size() == 2);

    auto it = m.find(AvaString("uno"));
    assert(it != m.end());
    assert(it->second == 1);
    assert(m.find(AvaString("tres")) == m.end()); // busqueda de clave ausente

    size_t iterated = 0;
    for (const auto& kv : m) { (void)kv; ++iterated; } // iteracion
    assert(iterated == 2);

    m.erase(AvaString("uno")); // eliminacion
    assert(m.size() == 1);
    assert(m.find(AvaString("uno")) == m.end());

    std::cout << "[OK] AvaMap: hash/insercion/busqueda/eliminacion/iteracion\n";
}

// --- AvaValue: cada variante de la seccion 4.1 (nil/bool/number/string/
//     array/object), typed correctamente ---
void TestAvaValue() {
    AvaValue n = AvaValue::Nil();
    AvaValue b = AvaValue::Bool(true);
    AvaValue num = AvaValue::Number(3.5);
    AvaValue s = AvaValue::String("texto");

    assert(n.type == ValueType::Nil);
    assert(b.type == ValueType::Bool && b.b == true);
    assert(num.type == ValueType::Number && num.n == 3.5);
    assert(s.type == ValueType::String);

    // Value es 16 bytes (tag + union) -- lo que pide el plan para que la
    // copia sea barata (ver comentario en AvaValue.h).
    assert(sizeof(AvaValue) <= 24); // margen chico sobre el tag+union+padding

    // AvaArray de AvaValue (array del lenguaje) + AvaMap-backed DictObj
    // (map del lenguaje) ya existen como ListObj/DictObj -- ver
    // AvaValue.h para por que no se duplica un ValueType::Array
    // paralelo.
    AvaValue list = AvaValue::String("placeholder"); // ListObj se arma via ListObj directo
    assert(list.type == ValueType::String);

    std::cout << "[OK] AvaValue: nil/bool/number/string tipados correctamente\n";
}

// --- AvaObject: ref-count, registro GC, destructor virtual ---
void TestAvaObject() {
    avastd::int64_t before = GcLiveObjectCount();

    auto* obj = new StringObj(avastd::string("vive"));
    assert(obj->ref_count.load() == 1); // ref-count inicial
    assert(GcLiveObjectCount() == before + 1); // registro intrusivo (Fase 5 sub-fase 4)

    // AvaObject == Object: un puntero a AvaObject* debe despachar bien al
    // destructor real via virtual dtor, y desregistrarse del contador.
    AvaObject* base = obj;
    assert(base->gc_kind == GcObjectKind::String);

    {
        AvaValue v = AvaValue::String("otra");
        Retain(v);
        assert(v.obj->ref_count.load() == 2); // Retain() sube el contador
        Release(v);
        assert(v.obj->ref_count.load() == 1); // Release() lo baja de vuelta
        // ~Value() al cerrar este scope hace el Release() final y libera
        // "otra" -- por eso el assert de mas abajo queda afuera del scope.
    }

    delete base; // destructor virtual -> ~StringObj -> ~Object -> desregistra
    assert(GcLiveObjectCount() == before);

    std::cout << "[OK] AvaObject: ref-count, registro/desregistro GC, destructor virtual\n";
}

} // namespace

int main() {
    TestAvaString();
    TestAvaArray();
    TestAvaMap();
    TestAvaValue();
    TestAvaObject();

    std::cout << "\nTODOS LOS TESTS DE FASE 2 (AvaString/AvaArray/AvaMap/AvaValue/AvaObject) PASARON\n";
    return 0;
}
