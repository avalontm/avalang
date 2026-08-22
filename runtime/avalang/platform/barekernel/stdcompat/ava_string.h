#ifndef AVA_STDCOMPAT_STRING_H
#define AVA_STDCOMPAT_STRING_H

#include "ava_platform_caps.h"
#include "ava_types.h"
#include "ava_utility.h"
#include "ava_new.h"
#include "../../AvaMemory.h"

#if AVA_HAVE_STD_LIBRARY

#include <string>
namespace avastd {
using string = std::string;
using std::to_string;
using std::stod;
}
#include <cstring>
namespace avastd { using std::strlen; using std::memcpy; using std::memcmp; }

#else

namespace avastd {

// String simple, sin small-string-optimization (SSO): siempre en heap via
// operator new/delete. No es lo mas rapido posible, pero es correcto y
// simple, que es lo que hace falta para desbloquear la compilacion. Si el
// profiling en el kernel real muestra que la alocacion de strings es un
// cuello de botella, SSO se puede agregar despues sin cambiar la API.
// Null-terminated internamente para que c_str() sea O(1) (AvaLang llama
// c_str() constantemente al cruzar la API C).

class string {
public:
    string() : data_(empty_buf()), len_(0), cap_(0), owns_(false) {}

    string(const char* s) {
        len_ = c_strlen(s);
        cap_ = len_;
        data_ = alloc_bytes(len_ + 1);
        for (avastd::size_t i = 0; i <= len_; ++i) data_[i] = s[i];
        owns_ = true;
    }

    string(const char* s, avastd::size_t n) {
        len_ = n;
        cap_ = n;
        data_ = alloc_bytes(n + 1);
        for (avastd::size_t i = 0; i < n; ++i) data_[i] = s[i];
        data_[n] = '\0';
        owns_ = true;
    }

    string(const string& other) : string(other.data_, other.len_) {}

    // Rango de iteradores (InputIt = puntero, unico caso real: proto_io.cpp
    // reconstruye un string desde vector<uint8_t>::const_iterator para
    // pasarlo a istringstream). No es un forward-iterator generico -- solo
    // punteros crudos, que es lo unico que produce avastd::vector::begin().
    template <class InputIt>
    string(InputIt first, InputIt last) {
        len_ = static_cast<avastd::size_t>(last - first);
        cap_ = len_;
        data_ = alloc_bytes(len_ + 1);
        avastd::size_t i = 0;
        for (InputIt it = first; it != last; ++it, ++i) data_[i] = static_cast<char>(*it);
        data_[len_] = '\0';
        owns_ = true;
    }

    // Fill constructor (count, ch) -- std::string(1, some_char) etc.
    string(avastd::size_t count, char ch) {
        len_ = count; cap_ = count;
        data_ = alloc_bytes(count + 1);
        for (avastd::size_t i = 0; i < count; ++i) data_[i] = ch;
        data_[count] = '\0';
        owns_ = true;
    }

    string(string&& other) noexcept
        : data_(other.data_), len_(other.len_), cap_(other.cap_), owns_(other.owns_) {
        other.data_ = empty_buf();
        other.len_ = 0;
        other.cap_ = 0;
        other.owns_ = false;
    }

    ~string() { free_if_owned(); }

    string& operator=(const string& other) {
        if (this == &other) return *this;
        assign(other.data_, other.len_);
        return *this;
    }
    string& operator=(string&& other) noexcept {
        if (this == &other) return *this;
        free_if_owned();
        data_ = other.data_; len_ = other.len_; cap_ = other.cap_; owns_ = other.owns_;
        other.data_ = empty_buf(); other.len_ = 0; other.cap_ = 0; other.owns_ = false;
        return *this;
    }
    string& operator=(const char* s) {
        assign(s, c_strlen(s));
        return *this;
    }

    void assign(const char* s, avastd::size_t n) {
        free_if_owned();
        data_ = alloc_bytes(n + 1);
        for (avastd::size_t i = 0; i < n; ++i) data_[i] = s[i];
        data_[n] = '\0';
        len_ = n;
        cap_ = n;
        owns_ = true;
    }

    string& operator+=(const string& other) { append(other.data_, other.len_); return *this; }
    string& operator+=(const char* s)       { append(s, c_strlen(s)); return *this; }
    string& operator+=(char c)              { append(&c, 1); return *this; }

    friend string operator+(const string& a, const string& b) {
        string r; r.reserve(a.len_ + b.len_);
        r.append(a.data_, a.len_);
        r.append(b.data_, b.len_);
        return r;
    }

    void append(const char* s, avastd::size_t n) {
        avastd::size_t new_len = len_ + n;
        if (new_len > cap_) grow_to(new_len);
        for (avastd::size_t i = 0; i < n; ++i) data_[len_ + i] = s[i];
        len_ = new_len;
        data_[len_] = '\0';
    }

    void reserve(avastd::size_t n) { if (n > cap_) grow_to(n); }
    void clear() { len_ = 0; if (data_) data_[0] = '\0'; }
    void pop_back() { if (len_ > 0) { --len_; data_[len_] = '\0'; } }
    // push_back: BareKernelConsole::ReadLine arma la linea caracter a
    // caracter leyendo del kernel -- hueco encontrado via el syntax-check
    // freestanding del backend real de BareKernel (ver §11 del audit).
    void push_back(char c) { append(&c, 1); }

    // resize: usado por proto_io.cpp::ReadString para reservar el buffer
    // antes de leer bytes crudos del stream (patron
    // s.resize(len); in.read(&s[0], len)). Rellena con `fill` al crecer,
    // trunca (sin liberar capacidad) al achicar -- igual que std::string.
    void resize(avastd::size_t n, char fill = '\0') {
        if (n > cap_) grow_to(n);
        if (n > len_) for (avastd::size_t i = len_; i < n; ++i) data_[i] = fill;
        len_ = n;
        if (data_) data_[len_] = '\0';
    }

    const char* c_str() const noexcept { return data_; }
    const char* data() const noexcept { return data_; }
    char* begin() noexcept { return data_; }
    char* end() noexcept { return data_ + len_; }
    const char* begin() const noexcept { return data_; }
    const char* end() const noexcept { return data_ + len_; }
    avastd::size_t size() const noexcept { return len_; }
    avastd::size_t length() const noexcept { return len_; }
    bool empty() const noexcept { return len_ == 0; }

    char&       operator[](avastd::size_t i)       { return data_[i]; }
    const char& operator[](avastd::size_t i) const { return data_[i]; }
    char&       back()       { return data_[len_ - 1]; }
    const char& back() const { return data_[len_ - 1]; }
    char&       front()       { return data_[0]; }
    const char& front() const { return data_[0]; }

    string substr(avastd::size_t pos, avastd::size_t count = static_cast<avastd::size_t>(-1)) const {
        if (pos >= len_) return string();
        avastd::size_t n = (count > len_ - pos) ? (len_ - pos) : count;
        return string(data_ + pos, n);
    }

    static constexpr avastd::size_t npos = static_cast<avastd::size_t>(-1);

    avastd::size_t find(char c, avastd::size_t start = 0) const {
        for (avastd::size_t i = start; i < len_; ++i) if (data_[i] == c) return i;
        return npos;
    }
    avastd::size_t find(const string& needle, avastd::size_t start = 0) const {
        if (needle.len_ == 0) return start <= len_ ? start : npos;
        if (needle.len_ > len_) return npos;
        for (avastd::size_t i = start; i + needle.len_ <= len_; ++i) {
            avastd::size_t j = 0;
            while (j < needle.len_ && data_[i + j] == needle.data_[j]) ++j;
            if (j == needle.len_) return i;
        }
        return npos;
    }

    // find_last_of(chars): ultima posicion de CUALQUIERA de los caracteres
    // en `chars` (equivalente a std::string::find_last_of(const char*)).
    // No confundir con find(const string&), que busca la subcadena
    // completa en secuencia.
    avastd::size_t find_last_of(const char* chars) const {
        for (avastd::size_t i = len_; i > 0; --i) {
            char c = data_[i - 1];
            for (const char* p = chars; *p; ++p) if (*p == c) return i - 1;
        }
        return npos;
    }
    avastd::size_t find_last_not_of(char c) const {
        for (avastd::size_t i = len_; i > 0; --i) {
            if (data_[i - 1] != c) return i - 1;
        }
        return npos;
    }

    // erase(pos): trunca desde pos hasta el final (uso mas comun en
    // AvaLang). erase(pos, count): elimina `count` chars desde pos.
    string& erase(avastd::size_t pos) {
        if (pos < len_) { len_ = pos; data_[len_] = '\0'; }
        return *this;
    }
    string& erase(avastd::size_t pos, avastd::size_t count) {
        if (pos >= len_) return *this;
        avastd::size_t end = (count > len_ - pos) ? len_ : pos + count;
        for (avastd::size_t i = end; i < len_; ++i) data_[pos + (i - end)] = data_[i];
        len_ -= (end - pos);
        data_[len_] = '\0';
        return *this;
    }

    int compare(const string& other) const {
        avastd::size_t n = len_ < other.len_ ? len_ : other.len_;
        for (avastd::size_t i = 0; i < n; ++i) {
            if (data_[i] != other.data_[i]) return (unsigned char)data_[i] - (unsigned char)other.data_[i];
        }
        if (len_ == other.len_) return 0;
        return len_ < other.len_ ? -1 : 1;
    }

    // compare(pos, count, other): compara el substring [pos, pos+count)
    // de *this contra `other` completo (equivalente al overload de
    // std::string::compare usado por builtin_str_startsWith/endsWith --
    // no soporta el resto de overloads de std::string::compare, solo el
    // que realmente se usa en el arbol).
    int compare(avastd::size_t pos, avastd::size_t count, const string& other) const {
        avastd::size_t avail = pos >= len_ ? 0 : len_ - pos;
        avastd::size_t n = (count > avail) ? avail : count;
        return string(data_ + (pos <= len_ ? pos : len_), n).compare(other);
    }

    // replace(pos, count, str): reemplaza [pos, pos+count) por `str`,
    // equivalente al overload de std::string::replace usado por
    // builtin_str_replace. No soporta el resto de overloads.
    string& replace(avastd::size_t pos, avastd::size_t count, const string& str) {
        if (pos > len_) pos = len_;
        avastd::size_t avail = len_ - pos;
        avastd::size_t remove_n = (count > avail) ? avail : count;
        avastd::size_t tail_len = len_ - (pos + remove_n);
        avastd::size_t new_len = pos + str.len_ + tail_len;
        char* new_data = alloc_bytes(new_len + 1);
        for (avastd::size_t i = 0; i < pos; ++i) new_data[i] = data_[i];
        for (avastd::size_t i = 0; i < str.len_; ++i) new_data[pos + i] = str.data_[i];
        for (avastd::size_t i = 0; i < tail_len; ++i) new_data[pos + str.len_ + i] = data_[pos + remove_n + i];
        new_data[new_len] = '\0';
        free_if_owned();
        data_ = new_data;
        len_ = new_len;
        cap_ = new_len;
        owns_ = true;
        return *this;
    }
    friend bool operator==(const string& a, const string& b) { return a.compare(b) == 0; }
    friend bool operator!=(const string& a, const string& b) { return a.compare(b) != 0; }
    friend bool operator<(const string& a, const string& b)  { return a.compare(b) < 0; }
    friend bool operator<=(const string& a, const string& b) { return a.compare(b) <= 0; }
    friend bool operator>(const string& a, const string& b)  { return a.compare(b) > 0; }
    friend bool operator>=(const string& a, const string& b) { return a.compare(b) >= 0; }
    friend bool operator==(const string& a, const char* b) { return a.compare(string(b)) == 0; }
    friend bool operator!=(const string& a, const char* b) { return a.compare(string(b)) != 0; }

    // Hash simple (FNV-1a) para uso con avastd::unordered_map<string, V>.
    // OJO: el target real (i686-elf) tiene size_t de 32 bits, no 64 --
    // usar la constante de offset/prime de FNV-1a de 64 bits ahi trunca
    // mal (probado: 14695981039346656037 truncado a 32 bits deja de ser
    // FNV-1a valido, solo un numero cualquiera). Se selecciona el set de
    // constantes correcto segun sizeof(avastd::size_t) en vez de asumir
    // 64 bits como hacia la primera version de este archivo.
    avastd::size_t hash() const noexcept {
        if constexpr (sizeof(avastd::size_t) == 8) {
            avastd::size_t h = 14695981039346656037ull;
            for (avastd::size_t i = 0; i < len_; ++i) {
                h ^= static_cast<unsigned char>(data_[i]);
                h *= 1099511628211ull;
            }
            return h;
        } else {
            avastd::size_t h = 2166136261u;
            for (avastd::size_t i = 0; i < len_; ++i) {
                h ^= static_cast<unsigned char>(data_[i]);
                h *= 16777619u;
            }
            return h;
        }
    }

private:
    static avastd::size_t c_strlen(const char* s) {
        avastd::size_t n = 0;
        while (s[n] != '\0') ++n;
        return n;
    }
    // alloc_bytes(total): reserva exactamente `total` bytes (incluye el
    // '\0' -- el caller ya suma +1). El atributo alloc_size(1) le dice a
    // GCC el tamano real del buffer devuelto; sin el, el chequeo estatico
    // de -Wstringop-overflow (activo en -O3, como el build Release real)
    // no podia inferirlo a traves de multiples niveles de inlining y
    // producia falsos positivos ("writing 1 byte into a region of size 0"
    // en escrituras perfectamente validas).
    __attribute__((malloc, alloc_size(1)))
    static char* alloc_bytes(avastd::size_t total) {
        return static_cast<char*>(ava_alloc(total));
    }
    static char* empty_buf() {
        static char buf[1] = {'\0'};
        return buf;
    }
    void grow_to(avastd::size_t needed) {
        avastd::size_t new_cap = cap_ == 0 ? 16 : cap_ * 2;
        if (new_cap < needed) new_cap = needed;
        char* new_data = alloc_bytes(new_cap + 1);
        for (avastd::size_t i = 0; i < len_; ++i) new_data[i] = data_[i];
        new_data[len_] = '\0';
        free_if_owned();
        data_ = new_data;
        cap_ = new_cap;
        owns_ = true;
    }
    void free_if_owned() {
        if (owns_ && data_ != empty_buf()) ava_free(data_);
    }

    char* data_;
    avastd::size_t len_;
    avastd::size_t cap_;
    bool owns_;
};

// strlen: BareKernelConsole::EmitAnsi la usa sobre literales C ("\033[0m"
// etc.) -- hueco encontrado via el syntax-check freestanding del backend
// real de BareKernel (ver §11 del audit). No via <cstring> (mismo
// problema que ya se resolvio en proto_io.h/obfuscate.h: ese header
// asume libc real, ausente con --without-headers).
inline avastd::size_t strlen(const char* s) {
    avastd::size_t n = 0;
    while (s[n] != '\0') ++n;
    return n;
}

// memcpy/memcmp: mismo hueco que strlen arriba, encontrado via el
// syntax-check freestanding del backend real de BareKernel al compilar
// compiler/proto_io.cpp -- necesita copiar un double a/desde un buffer de
// bytes crudo (WriteF64/ReadF64, sin violar strict aliasing) y comparar
// el magic de 4 bytes de un .avbc (ReadProto). El comentario de mas
// arriba sobre "ya resuelto en proto_io.h/obfuscate.h" se referia solo a
// que esos *headers* no incluyen <cstring> -- los .cpp si lo incluian
// crudo, que es exactamente lo que este toolchain (--without-headers) no
// tiene. Implementacion manual byte a byte, no via <cstring>.
inline void* memcpy(void* dst, const void* src, avastd::size_t n) {
    unsigned char* d = static_cast<unsigned char*>(dst);
    const unsigned char* s = static_cast<const unsigned char*>(src);
    for (avastd::size_t i = 0; i < n; ++i) d[i] = s[i];
    return dst;
}
inline int memcmp(const void* a, const void* b, avastd::size_t n) {
    const unsigned char* pa = static_cast<const unsigned char*>(a);
    const unsigned char* pb = static_cast<const unsigned char*>(b);
    for (avastd::size_t i = 0; i < n; ++i) {
        if (pa[i] != pb[i]) return static_cast<int>(pa[i]) - static_cast<int>(pb[i]);
    }
    return 0;
}

inline string to_string(long long v) {
    char buf[24];
    int i = 23;
    buf[i] = '\0';
    bool neg = v < 0;
    unsigned long long u = neg ? static_cast<unsigned long long>(-(v + 1)) + 1 : static_cast<unsigned long long>(v);
    do {
        buf[--i] = static_cast<char>('0' + (u % 10));
        u /= 10;
    } while (u != 0);
    if (neg) buf[--i] = '-';
    return string(&buf[i]);
}
inline string to_string(int v) { return to_string(static_cast<long long>(v)); }
inline string to_string(avastd::uint64_t v) {
    char buf[24];
    int i = 23;
    buf[i] = '\0';
    do { buf[--i] = static_cast<char>('0' + (v % 10)); v /= 10; } while (v != 0);
    return string(&buf[i]);
}
// size_t: en el target real (i686-elf, __SIZE_TYPE__ = "unsigned int")
// es un tipo distinto de int/long long/uint64_t, asi que sin este
// overload avastd::to_string(size_t) ambigua entre los tres -- mismo
// problema que resolvio el overload de double para ui/builtins.cpp, esta
// vez encontrado en compiler/obfuscate.cpp (FlattenProtoControlFlow llama
// to_string sobre un indice size_t). Redirige al overload de uint64_t en
// vez de duplicar la logica de formateo.
//
// Va detras de SFINAE (no un `inline string to_string(avastd::size_t)`
// liso) porque en hosts LP64 -- incluido el x86_64 usado como sustituto
// de sintaxis de este entorno sin i686-elf-g++, ver
// RUNTIME_CORE_AUDIT.md Seccion 11.2 -- size_t y uint64_t son el MISMO
// tipo ("unsigned long"), y un overload liso chocaria como redefinicion
// exacta del to_string(uint64_t) de arriba. El `enable_if` lo desactiva
// justo en ese caso, donde ya no hace falta (el overload de uint64_t ya
// cubre a size_t directo).
template <class SizeT,
          class = avastd::enable_if_t<avastd::is_same<SizeT, avastd::size_t>::value &&
                                       !avastd::is_same<SizeT, avastd::uint64_t>::value>>
inline string to_string(SizeT v) { return to_string(static_cast<avastd::uint64_t>(v)); }
// double: sin snprintf/libc, formato fijo de 6 decimales (mismo default
// que std::to_string(double) real) via parte entera + parte fraccionaria
// multiplicada y truncada. Encontrado como hueco real en ui/builtins.cpp
// (ToLogString con numero no entero) -- antes ambiguaba entre
// to_string(long long)/to_string(uint64_t), ninguno de los dos pensado
// para doubles.
inline string to_string(double v) {
    bool neg = v < 0;
    if (neg) v = -v;
    unsigned long long ip = static_cast<unsigned long long>(v);
    double frac = v - static_cast<double>(ip);
    unsigned long long fp = static_cast<unsigned long long>(frac * 1000000.0 + 0.5);
    if (fp >= 1000000ull) { fp -= 1000000ull; ++ip; }
    string out;
    if (neg) out += '-';
    out += to_string(static_cast<avastd::uint64_t>(ip));
    out += '.';
    char frac_buf[7];
    for (int i = 5; i >= 0; --i) { frac_buf[i] = static_cast<char>('0' + (fp % 10)); fp /= 10; }
    frac_buf[6] = '\0';
    out += frac_buf;
    return out;
}

// stod: parseo manual (sin strtod/libc). Soporta signo, parte entera,
// parte fraccionaria y exponente opcional (e/E). No valida overflow ni
// reproduce el redondeo bit-a-bit de strtod real -- suficiente para los
// literales numericos que emite el propio compilador de AvaLang.
inline double stod(const string& s, avastd::size_t* pos = nullptr) {
    avastd::size_t i = 0;
    avastd::size_t n = s.size();
    while (i < n && (s[i] == ' ' || s[i] == '\t')) ++i;
    bool neg = false;
    if (i < n && (s[i] == '+' || s[i] == '-')) { neg = (s[i] == '-'); ++i; }
    double result = 0.0;
    while (i < n && s[i] >= '0' && s[i] <= '9') { result = result * 10.0 + (s[i] - '0'); ++i; }
    if (i < n && s[i] == '.') {
        ++i;
        double frac = 0.1;
        while (i < n && s[i] >= '0' && s[i] <= '9') {
            result += (s[i] - '0') * frac;
            frac *= 0.1;
            ++i;
        }
    }
    if (i < n && (s[i] == 'e' || s[i] == 'E')) {
        avastd::size_t save = i;
        avastd::size_t j = i + 1;
        bool eneg = false;
        if (j < n && (s[j] == '+' || s[j] == '-')) { eneg = (s[j] == '-'); ++j; }
        avastd::size_t digits_start = j;
        int exp = 0;
        while (j < n && s[j] >= '0' && s[j] <= '9') { exp = exp * 10 + (s[j] - '0'); ++j; }
        if (j == digits_start) {
            i = save;
        } else {
            double base = eneg ? 0.1 : 10.0;
            for (int k = 0; k < exp; ++k) result *= base;
            i = j;
        }
    }
    if (neg) result = -result;
    if (pos) *pos = i;
    return result;
}

}  // namespace avastd

#endif  // AVA_HAVE_STD_LIBRARY

#endif  // AVA_STDCOMPAT_STRING_H
