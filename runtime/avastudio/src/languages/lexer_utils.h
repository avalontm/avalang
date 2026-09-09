#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "languages/function_index.h"

namespace studio::lexer {

bool IsIdentStart(char c);
bool IsIdentChar(char c);

std::string ReadIdent(const std::string& text, size_t& i);

void SkipInlineWhitespace(const std::string& text, size_t& i);

std::vector<std::string> SplitParams(const std::string& raw);

std::string BuildDisplay(const std::string& name, const std::vector<std::string>& params);

std::string TrimTrailing(const std::string& s);

bool ParseParamLine(const std::string& line, std::string& name, std::string& desc);

void ApplyDocBlock(FunctionSignature& sig, const std::vector<std::string>& pending_doc);

std::string ParseReturnTypeAnnotation(const std::string& text, size_t& i, size_t end);

std::string InferReturnTypeFromBody(const std::string& text, size_t body_start, size_t body_end);

std::string ResolveImportPath(const std::vector<std::string>& module_path,
                               const std::string& current_file_dir,
                               const std::string& stdlib_dir = "");

}
