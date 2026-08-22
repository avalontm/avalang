#ifndef AVA_STDCOMPAT_UNORDERED_MAP_H
#define AVA_STDCOMPAT_UNORDERED_MAP_H

#include "ava_platform_caps.h"
#include "ava_types.h"
#include "ava_utility.h"
#include "ava_vector.h"
#include "ava_string.h"
#include "ava_error.h"
#include "ava_new.h"

#if AVA_HAVE_STD_LIBRARY

#include <unordered_map>
#include <unordered_set>
namespace avastd {
template <class K, class V> using unordered_map = std::unordered_map<K, V>;
template <class K> using unordered_set = std::unordered_set<K>;
}

#else

namespace avastd {

// Traits de hash: AvaLang solo instancia unordered_map/set con
// avastd::string como clave (ver grep en vm.h/value.h/module.h) mas
// algunos enteros sueltos -- se cubren ambos casos. Si aparece una clave
// nueva en fases siguientes, se agrega una especializacion aca (no hay que
// tocar el hash table en si).
template <class K> struct Hash;
template <> struct Hash<string> {
    avastd::size_t operator()(const string& s) const noexcept { return s.hash(); }
};
template <> struct Hash<int> {
    avastd::size_t operator()(int v) const noexcept { return static_cast<avastd::size_t>(v) * 2654435761u; }
};
template <> struct Hash<avastd::int64_t> {
    // Constante de Fibonacci hashing de 64 bits: en un size_t de 32 bits
    // (target real, ver nota en ava_string.h::hash()) el compilador la
    // trunca al multiplicar, lo cual sigue siendo un multiplicador impar
    // valido para hashing multiplicativo (no rompe la propiedad, a
    // diferencia del offset-basis de FNV que si necesita el valor exacto
    // para su ancho). No instanciado hoy por AvaLang (unordered_map solo
    // usa claves avastd::string en vm.h/value.h/module.h) -- se deja
    // correcto igual para cuando una fase futura lo necesite.
    avastd::size_t operator()(avastd::int64_t v) const noexcept { return static_cast<avastd::size_t>(v) * 0x9E3779B97F4A7C15ull; }
};
// Punteros: gc_trace.cpp/gc_sweep.cpp usan unordered_set<Object*> para el
// marcado del GC (ver §11 del audit) -- no cubierto por la nota original
// de "solo string + enteros sueltos". Generico para cualquier T*, mismo
// multiplicador Fibonacci que Hash<int64_t>.
template <class T> struct Hash<T*> {
    avastd::size_t operator()(T* p) const noexcept {
        return reinterpret_cast<avastd::size_t>(p) * 0x9E3779B97F4A7C15ull;
    }
};

// Tabla hash encadenada (buckets = vector<vector<pair<K,V>>>) -- prioriza
// simplicidad/corrección sobre performance de punta; AvaLang no hace
// millones de lookups por frame en el VM, así que esto no es el cuello de
// botella esperado. Si lo fuera, se reemplaza por open-addressing sin
// tocar la API (find/operator[]/erase/begin/end).
template <class K, class V, class H = Hash<K>>
class unordered_map {
    struct Entry { K first; V second; };
public:
    unordered_map() : buckets_(INITIAL_BUCKETS), count_(0) {}

    V& operator[](const K& key) {
        avastd::size_t idx = bucket_index(key);
        for (auto& e : buckets_[idx]) if (e.first == key) return e.second;
        maybe_rehash();
        idx = bucket_index(key);
        buckets_[idx].push_back(Entry{key, V()});
        ++count_;
        return buckets_[idx].back().second;
    }

    // emplace(key, value): inserta si la clave no existe todavia; no hace
    // nada si ya existe (misma semantica que std::unordered_map::emplace
    // -- no sobreescribe). AvaLang no usa el valor de retorno
    // (pair<iterator,bool>) en ningun lado, asi que se devuelve solo el
    // bool para mantener esto simple.
    bool emplace(const K& key, const V& value) {
        if (contains(key)) return false;
        maybe_rehash();
        buckets_[bucket_index(key)].push_back(Entry{key, value});
        ++count_;
        return true;
    }

    // at(key): como operator[] pero NO inserta si falta -- aborta via
    // AVA_THROW en vez de silenciosamente crear una entrada, igual que el
    // std::unordered_map::at real (que lanza std::out_of_range).
    V& at(const K& key) {
        V* p = find_ptr(key);
        if (!p) AVA_THROW(avastd::runtime_error("avastd::unordered_map::at: key not found"));
        return *p;
    }
    const V& at(const K& key) const {
        const V* p = find_ptr(key);
        if (!p) AVA_THROW(avastd::runtime_error("avastd::unordered_map::at: key not found"));
        return *p;
    }

private:
    V* find_ptr(const K& key) {
        auto& bucket = buckets_[bucket_index(key)];
        for (auto& e : bucket) if (e.first == key) return &e.second;
        return nullptr;
    }
    const V* find_ptr(const K& key) const {
        const auto& bucket = buckets_[bucket_index(key)];
        for (const auto& e : bucket) if (e.first == key) return &e.second;
        return nullptr;
    }

public:

    bool contains(const K& key) const {
        const auto& bucket = buckets_[bucket_index(key)];
        for (const auto& e : bucket) if (e.first == key) return true;
        return false;
    }

    // erase(key) primero -- lo necesitan find()/end() de abajo para poder
    // referirse a "iterator" antes de que la clase este completa.
    bool erase(const K& key) {
        auto& bucket = buckets_[bucket_index(key)];
        for (auto it = bucket.begin(); it != bucket.end(); ++it) {
            if (it->first == key) { bucket.erase(it); --count_; return true; }
        }
        return false;
    }


    avastd::size_t size() const noexcept { return count_; }
    bool empty() const noexcept { return count_ == 0; }
    void clear() { for (auto& b : buckets_) b.clear(); count_ = 0; }

    // Iteracion plana estilo std:: (for (auto& [k, v] : map)) -- necesaria
    // porque AvaLang recorre globals_/tablas de simbolos con range-for.
    class iterator {
    public:
        iterator(unordered_map* m, avastd::size_t bi, avastd::size_t ei) : m_(m), bi_(bi), ei_(ei) { skip_empty(); }
        Entry& operator*() { return m_->buckets_[bi_][ei_]; }
        Entry* operator->() { return &m_->buckets_[bi_][ei_]; }
        iterator& operator++() { ++ei_; skip_empty(); return *this; }
        bool operator!=(const iterator& o) const { return bi_ != o.bi_ || ei_ != o.ei_; }
        bool operator==(const iterator& o) const { return !(*this != o); }
    private:
        friend class unordered_map;
        void skip_empty() {
            while (bi_ < m_->buckets_.size() && ei_ >= m_->buckets_[bi_].size()) {
                ++bi_; ei_ = 0;
            }
        }
        unordered_map* m_;
        avastd::size_t bi_, ei_;
    };
    iterator begin() { return iterator(this, 0, 0); }
    iterator end()   { return iterator(this, buckets_.size(), 0); }

    // find() al estilo std:: (compara con end() en vez de nullptr) -- es
    // lo que permite migrar mecanicamente codigo existente del patron
    // `auto it = map.find(k); if (it != map.end()) use(it->second);`
    iterator find(const K& key) {
        avastd::size_t bi = bucket_index(key);
        auto& bucket = buckets_[bi];
        for (avastd::size_t ei = 0; ei < bucket.size(); ++ei) {
            if (bucket[ei].first == key) return iterator(this, bi, ei);
        }
        return end();
    }

    // erase(iterator): variante estilo std:: -- borra la entrada que
    // apunta el iterador y devuelve un iterador a la SIGUIENTE entrada
    // (no a end() salvo que efectivamente no quede nada despues). Critico
    // para el patron `for (it = begin(); it != end();) { if (cond) it =
    // erase(it); else ++it; }` (ver MemoryFileSystem.cpp) -- devolver
    // siempre end() cortaria la iteracion antes de tiempo y dejaria
    // entradas sin visitar. bucket.erase() ya desplaza los elementos
    // siguientes del mismo bucket un lugar hacia atras, asi que
    // reconstruir el iterador con el mismo (bi_, ei_) despues de borrar
    // apunta exactamente a lo que antes era el siguiente elemento (el
    // constructor de iterator hace skip_empty() solo si hace falta saltar
    // de bucket).
    iterator erase(iterator pos) {
        avastd::size_t bi = pos.bi_;
        avastd::size_t ei = pos.ei_;
        buckets_[bi].erase(buckets_[bi].begin() + ei);
        --count_;
        return iterator(this, bi, ei);
    }

    // const_iterator es exactamente el mismo tipo que iterator (esta
    // implementacion no distingue mutable/const-view, a diferencia de
    // std::unordered_map real) -- alcanza para el uso que hace AvaLang
    // (comparar contra end(), leer ->second), y evita duplicar toda la
    // maquinaria del iterador solo para la variante const.
    using const_iterator = iterator;
    const_iterator find(const K& key) const {
        return const_cast<unordered_map*>(this)->find(key);
    }
    const_iterator begin() const { return const_cast<unordered_map*>(this)->begin(); }
    const_iterator end() const { return const_cast<unordered_map*>(this)->end(); }

private:
    static constexpr avastd::size_t INITIAL_BUCKETS = 16;

    avastd::size_t bucket_index(const K& key) const {
        return H{}(key) % buckets_.size();
    }
    void maybe_rehash() {
        if (count_ + 1 <= buckets_.size() * 2) return;
        vector<vector<Entry>> old = avastd::move(buckets_);
        buckets_ = vector<vector<Entry>>(old.size() * 2);
        for (auto& bucket : old) {
            for (auto& e : bucket) {
                buckets_[H{}(e.first) % buckets_.size()].push_back(avastd::move(e));
            }
        }
    }

    vector<vector<Entry>> buckets_;
    avastd::size_t count_;
};

template <class K, class H = Hash<K>>
class unordered_set {
public:
    unordered_set() : map_() {}
    void insert(const K& key) { map_[key] = true; }
    bool contains(const K& key) const { return map_.contains(key); }
    bool erase(const K& key) { return map_.erase(key); }
    avastd::size_t size() const noexcept { return map_.size(); }
    bool empty() const noexcept { return map_.empty(); }
    void clear() { map_.clear(); }

    // find()/end() al estilo std:: -- delega al mapa interno; el "valor"
    // de la entrada (bool) no importa, solo si la clave esta. Cubre el
    // patron `set.find(x) != set.end()` (ver module.cpp) sin duplicar la
    // logica de iteracion del hash table.
    using iterator = typename unordered_map<K, bool, H>::iterator;
    iterator find(const K& key) { return map_.find(key); }
    iterator end() { return map_.end(); }
    iterator begin() { return map_.begin(); }
private:
    unordered_map<K, bool, H> map_;
};

}  // namespace avastd

#endif  // AVA_HAVE_STD_LIBRARY

#endif  // AVA_STDCOMPAT_UNORDERED_MAP_H
