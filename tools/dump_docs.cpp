// Standalone tool -- NOT part of the ava_studio build (not listed in
// studio/CMakeLists.txt's STUDIO_SOURCES, no ImGui/GLFW dependency at
// all). Its only job is to serialize DefaultKeywordDocs()/
// DefaultBuiltinSignatures() -- the same content that used to be the
// only source of truth, hardcoded in keyword_docs.cpp/builtin_signatures.cpp
// -- into the two CSV files Ava Studio now reads at runtime
// (data/keyword_docs.csv, data/builtin_signatures.csv). Run it once to
// bootstrap those files; from then on the CSVs are what you edit, and
// re-running this tool would overwrite any hand edits, so it's meant to
// be a one-time step, not part of the normal build.
//
// Build standalone, e.g. from the studio/ directory:
//   g++ -std=c++17 -Isrc ../tools/dump_docs.cpp src/languages/keyword_docs.cpp \
//       src/languages/builtin_signatures.cpp src/util/csv.cpp src/util/data_dir.cpp \
//       -o dump_docs
//   ./dump_docs data/keyword_docs.csv data/builtin_signatures.csv

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "languages/builtin_signatures.h"
#include "languages/keyword_docs.h"
#include "util/csv.h"

namespace {

void WriteKeywordDocsCsv(const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    out << studio::util::WriteCsvRow({"name", "syntax", "example", "doc"}) << "\n";

    // Sorted by name (not insertion order) so re-running this tool
    // produces a deterministic diff instead of shuffling rows around
    // based on unordered_map iteration order.
    std::vector<const std::pair<const std::string, studio::KeywordDoc>*> entries;
    for (const auto& kv : studio::DefaultKeywordDocs()) entries.push_back(&kv);
    std::sort(entries.begin(), entries.end(),
              [](const auto* a, const auto* b) { return a->first < b->first; });

    for (const auto* kv : entries) {
        const studio::KeywordDoc& doc = kv->second;
        std::vector<std::string> escaped_syntax;
        for (const auto& variant : doc.syntax) {
            std::string escaped = variant;
            std::string with_escapes;
            for (char c : escaped) {
                if (c == '\n') with_escapes += "\\n";
                else with_escapes += c;
            }
            escaped_syntax.push_back(with_escapes);
        }
        std::string syntax_field = studio::util::JoinOn(escaped_syntax, "|||");

        std::string example_field;
        for (char c : doc.example) {
            if (c == '\n') example_field += "\\n";
            else example_field += c;
        }

        out << studio::util::WriteCsvRow({doc.name, syntax_field, example_field, doc.doc}) << "\n";
    }
    std::cout << "Wrote " << entries.size() << " keywords to " << path << "\n";
}

void WriteBuiltinSignaturesCsv(const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    out << studio::util::WriteCsvRow({"name", "params", "doc"}) << "\n";

    std::vector<const std::pair<const std::string, studio::FunctionSignature>*> entries;
    for (const auto& kv : studio::DefaultBuiltinSignatures()) entries.push_back(&kv);
    std::sort(entries.begin(), entries.end(),
              [](const auto* a, const auto* b) { return a->first < b->first; });

    for (const auto* kv : entries) {
        const studio::FunctionSignature& sig = kv->second;
        std::string params_field = studio::util::JoinOn(sig.params, "|");
        out << studio::util::WriteCsvRow({sig.name, params_field, sig.doc}) << "\n";
    }
    std::cout << "Wrote " << entries.size() << " builtins to " << path << "\n";
}

} // namespace

int main(int argc, char** argv) {
    std::string keyword_docs_path = argc > 1 ? argv[1] : "keyword_docs.csv";
    std::string builtin_signatures_path = argc > 2 ? argv[2] : "builtin_signatures.csv";
    WriteKeywordDocsCsv(keyword_docs_path);
    WriteBuiltinSignaturesCsv(builtin_signatures_path);
    return 0;
}
