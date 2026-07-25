#pragma once

#include <string>
#include <vector>

namespace studio::util {

// Parser CSV mínimo pero correcto (RFC4180): soporta campos entre comillas
// dobles, comas y saltos de línea reales dentro de un campo citado, y ""
// como forma de escapar una comilla literal dentro de un campo citado.
// Usado por keyword_docs.cpp/builtin_signatures.cpp para leer
// data/keyword_docs.csv y data/builtin_signatures.csv sin depender de una
// librería externa.
//
// No hace ninguna interpretación semántica de las celdas -- en particular,
// NO desescapa "\n" literal (barra + n) a salto de línea real; eso es
// responsabilidad del caller (ver UnescapeCell más abajo), porque solo el
// caller sabe qué columnas de qué archivo usan esa convención.
//
// Devuelve una fila vacía por cada línea en blanco del archivo (útil para
// separar visualmente secciones al editar a mano); el caller debería
// saltarlas si corresponde.
std::vector<std::vector<std::string>> ParseCsv(const std::string& text);

// Arma una línea CSV bien formada a partir de campos crudos: envuelve en
// comillas cualquier campo que contenga coma, comilla o salto de línea, y
// duplica las comillas internas. Usado por tools/dump_docs.cpp para
// generar los CSV iniciales a partir de las tablas hardcodeadas de hoy.
std::string WriteCsvRow(const std::vector<std::string>& fields);

// Convención compartida por ambos CSV: dentro de una celda, "\n" (dos
// caracteres, barra invertida + n) representa un salto de línea real en
// el texto mostrado en el tooltip. Se usa una barra literal en vez de un
// salto de línea real embebido para que una fila = una línea física del
// archivo (más fácil de leer/diffear/editar a mano que una celda citada
// multi-línea).
std::string UnescapeCell(const std::string& raw);
std::string EscapeCell(const std::string& value);

// Segunda convención, solo para columnas que representan una lista de
// variantes/valores dentro de UNA celda ya citada por ParseCsv (ej. las
// dos formas de sintaxis de `while`, o la lista de parámetros de un
// builtin): separador "|||" para variantes de sintaxis completas, "|"
// simple para listas cortas de tokens (parámetros). Ninguno de los dos
// aparece nunca en el contenido real de AvaLang, así que no hace falta
// escapar nada adicional.
std::vector<std::string> SplitOn(const std::string& text, const std::string& separator);
std::string JoinOn(const std::vector<std::string>& parts, const std::string& separator);

} // namespace studio::util
