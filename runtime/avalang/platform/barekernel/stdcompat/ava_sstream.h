#ifndef AVA_STDCOMPAT_SSTREAM_H
#define AVA_STDCOMPAT_SSTREAM_H

#include "ava_platform_caps.h"
#include "ava_types.h"
#include "ava_string.h"
#include "ava_math.h"

#if AVA_HAVE_STD_LIBRARY

#include <sstream>
#include <ios>
#include <iomanip>
#include <string>
namespace avastd {
using ios = std::ios;
using std::ostream;
using std::istream;
using std::ostringstream;
using std::istringstream;
using std::stringstream;
using std::streamsize;
using std::hex;
using std::setw;
using std::setfill;
using std::setprecision;
using std::getline;
}

#else

namespace avastd {

using streamsize = long long;

struct ios {
    static constexpr int binary = 1;
    static constexpr int in = 2;
    static constexpr int out = 4;
};

struct SetW { int w; };
inline SetW setw(int w) { return SetW{w}; }
struct SetFill { char c; };
inline SetFill setfill(char c) { return SetFill{c}; }
struct SetPrecision { int p; };
inline SetPrecision setprecision(int p) { return SetPrecision{p}; }
struct HexTag {};
inline constexpr HexTag hex{};

// ostream de solo memoria: no hay archivos reales en freestanding, todo
// stream de salida termina en un buffer avastd::string (ver proto_io.cpp/
// obfuscate.cpp, que solo usan ostringstream). Formatea numeros en
// notacion fija (no cientifica); suficiente para display/serializacion,
// no es un reemplazo bit-a-bit de printf("%g").
class ostream {
public:
    ostream() = default;
    explicit ostream(int) {}

    ostream& put(char c) { buf_ += c; return *this; }
    ostream& write(const char* s, avastd::size_t n) {
        for (avastd::size_t i = 0; i < n; ++i) buf_ += s[i];
        return *this;
    }
    string str() const { return buf_; }
    explicit operator bool() const { return true; }
    bool operator!() const { return false; }

    ostream& operator<<(const char* s) { buf_ += s; return *this; }
    ostream& operator<<(const string& s) { buf_ += s; return *this; }
    ostream& operator<<(char c) { buf_ += c; return *this; }
    ostream& operator<<(HexTag) { hex_ = true; return *this; }
    ostream& operator<<(SetW w) { width_ = w.w; return *this; }
    ostream& operator<<(SetFill f) { fill_ = f.c; return *this; }
    ostream& operator<<(SetPrecision p) { precision_ = p.p; return *this; }
    ostream& operator<<(avastd::uint64_t v) { write_uint(v); return *this; }
    ostream& operator<<(avastd::uint32_t v) { write_uint(static_cast<avastd::uint64_t>(v)); return *this; }
    ostream& operator<<(int v) {
        if (v < 0) { buf_ += '-'; write_uint(static_cast<avastd::uint64_t>(-(long long)v)); }
        else write_uint(static_cast<avastd::uint64_t>(v));
        return *this;
    }
    ostream& operator<<(double v) { write_double(v); return *this; }

private:
    void write_uint(avastd::uint64_t v) {
        char tmp[32];
        int n = 0;
        if (hex_) {
            if (v == 0) tmp[n++] = '0';
            while (v != 0) {
                int d = static_cast<int>(v & 0xF);
                tmp[n++] = d < 10 ? static_cast<char>('0' + d) : static_cast<char>('a' + d - 10);
                v >>= 4;
            }
        } else {
            if (v == 0) tmp[n++] = '0';
            while (v != 0) { tmp[n++] = static_cast<char>('0' + (v % 10)); v /= 10; }
        }
        for (int i = 0; i < n / 2; ++i) { char t = tmp[i]; tmp[i] = tmp[n - 1 - i]; tmp[n - 1 - i] = t; }
        int pad = width_ - n;
        for (int i = 0; i < pad; ++i) buf_ += fill_;
        for (int i = 0; i < n; ++i) buf_ += tmp[i];
        width_ = 0;
        fill_ = ' ';
    }
    void write_double(double v) {
        if (avastd::isnan(v)) { buf_ += "nan"; return; }
        if (avastd::isinf(v)) { buf_ += (v < 0 ? "-inf" : "inf"); return; }
        if (v < 0) { buf_ += '-'; v = -v; }
        double ip_d = avastd::floor(v);
        double fp = v - ip_d;
        string ip_str = avastd::to_string(static_cast<avastd::uint64_t>(ip_d));
        buf_ += ip_str;
        int digits = precision_ > 0 ? precision_ : 6;
        digits -= static_cast<int>(ip_str.size());
        if (digits <= 0) digits = 1;
        char frac[24];
        int n = 0;
        for (int i = 0; i < digits && n < 23; ++i) {
            fp *= 10.0;
            int d = static_cast<int>(fp);
            if (d > 9) d = 9;
            if (d < 0) d = 0;
            frac[n++] = static_cast<char>('0' + d);
            fp -= d;
        }
        while (n > 0 && frac[n - 1] == '0') --n;
        if (n > 0) {
            buf_ += '.';
            for (int i = 0; i < n; ++i) buf_ += frac[i];
        }
        width_ = 0;
    }

    string buf_;
    bool hex_ = false;
    int width_ = 0;
    char fill_ = ' ';
    int precision_ = 6;
};

using ostringstream = ostream;
using stringstream = ostream;

class istream {
public:
    istream() : data_(), pos_(0), fail_(false) {}
    explicit istream(const string& s) : data_(s), pos_(0), fail_(false) {}
    istream(const string& s, int) : data_(s), pos_(0), fail_(false) {}

    int get() {
        if (pos_ >= data_.size()) { fail_ = true; return -1; }
        return static_cast<unsigned char>(data_[pos_++]);
    }
    istream& read(char* dst, avastd::size_t n) {
        if (pos_ + n > data_.size()) { fail_ = true; return *this; }
        for (avastd::size_t i = 0; i < n; ++i) dst[i] = data_[pos_ + i];
        pos_ += n;
        return *this;
    }
    explicit operator bool() const { return !fail_; }
    bool operator!() const { return fail_; }

private:
    string data_;
    avastd::size_t pos_;
    bool fail_;
};
using istringstream = istream;

inline bool getline(istream& in, string& line, char delim = '\n') {
    line.clear();
    int c = in.get();
    if (c == -1) return false;
    while (c != -1 && static_cast<char>(c) != delim) {
        line += static_cast<char>(c);
        c = in.get();
    }
    return true;
}

}  // namespace avastd

#endif  // AVA_HAVE_STD_LIBRARY

#endif  // AVA_STDCOMPAT_SSTREAM_H
