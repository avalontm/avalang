const fs = require('fs');
const path = require('path');

const KEYWORD_DOCS = {
    if: { syntax: ['if condition then\n    ...\nelif other_condition then\n    ...\nelse\n    ...\nend'],
        doc: 'Corre el primer bloque cuya condicion sea verdadera. elif y else son opcionales; el bloque siempre cierra con end.' },
    then: { syntax: ['if condition then\n    ...\nend'],
        doc: 'Inicia el cuerpo de un if, elif, for, select o case. Siempre sigue a la condicion.' },
    elif: { syntax: ['if condition then\n    ...\nelif other_condition then\n    ...\nend'],
        doc: 'Agrega otra condicion a un if, evaluada solo si las anteriores fueron falsas.' },
    else: { syntax: ['if condition then\n    ...\nelse\n    ...\nend'],
        doc: 'Corre cuando ninguna condicion if/elif de arriba fue verdadera. Debe ser la ultima clausula antes de end.' },
    end: { syntax: ['if / while / for / func / class / try / select / extern\n    ...\nend'],
        doc: 'Cierra el bloque abierto por if, while, for, func, class, try, select o extern. Cada uno de esos necesita su end.' },
    while: { syntax: ['while condition\n    ...\nend', 'while (condition)\n    ...\nend'],
        doc: 'Repite el bloque mientras condition sea verdadera. Los parentesis son opcionales.' },
    for: { syntax: ['for item in iterable then\n    ...\nend', 'for i = inicio to fin (step paso)? then\n    ...\nend'],
        doc: 'Itera sobre una lista/range (estilo Python) o cuenta con i = inicio to fin (estilo VB6). Necesita then antes del cuerpo.' },
    in: { syntax: ['for item in iterable then ... end'], doc: 'Empareja con for para nombrar sobre que se itera.' },
    func: { syntax: ['func nombre(parametros)\n    ...\nend', 'func nombre(parametros) as TipoRetorno\n    ...\nend'],
        doc: 'Declara una funcion con nombre. as TipoRetorno es opcional (anotacion de tipo de retorno).' },
    class: { syntax: ['class Nombre\n    ...\nend', 'class Nombre : Base\n    ...\nend'],
        doc: 'Declara una clase. : Base es opcional y le da una unica superclase.' },
    new: { syntax: ['new Clase(args)'],
        doc: 'Crea una instancia nueva de Clase, corriendo su constructor. Obligatorio para instanciar: Clase(args) sin new ya no compila.' },
    base: { syntax: ['base(args)', 'base.Metodo(args)'],
        doc: 'Dentro de una subclase, llama al constructor o metodo de la superclase. AvaLang usa base(...), no super(...).' },
    this: { syntax: ['this.atributo', 'this.metodo(args)'],
        doc: 'Referencia a la instancia actual dentro de un metodo. Es implicita: nunca se declara como parametro. self funciona como alias.' },
    self: { syntax: ['self.atributo'], doc: 'Alias de this en la mayoria de los caminos del compilador.' },
    return: { syntax: ['return', 'return valor'], doc: 'Sale de la funcion actual, opcionalmente devolviendo valor (nil si se omite).' },
    break: { syntax: ['break'], doc: 'Sale del while/for mas interno inmediatamente.' },
    continue: { syntax: ['continue'], doc: 'Salta a la siguiente iteracion del while/for mas interno.' },
    pass: { syntax: ['pass'], doc: 'No hace nada. Placeholder util para un bloque que todavia esta vacio.' },
    import: { syntax: ['import modulo', 'import modulo.submodulo', 'import modulo as alias'],
        doc: 'Carga otro archivo .ava por nombre de modulo y expone sus nombres de nivel superior, opcionalmente bajo alias.' },
    as: { syntax: ['import modulo as alias', 'nombre as Tipo'], doc: 'Da un alias a un import, o anota el tipo de una variable/parametro/retorno.' },
    local: { syntax: ['local nombre = valor'], doc: 'Declara nombre como variable nueva en el scope actual en vez de asignar a una externa con el mismo nombre.' },
    raise: { syntax: ['raise valor'], doc: 'Lanza valor como excepcion, hasta que un catch la maneje.' },
    try: { syntax: ['try\n    ...\ncatch (e)\n    ...\nfinally\n    ...\nend'],
        doc: 'Corre el bloque, enviando cualquier excepcion a un catch. Se necesita al menos un catch o un finally.' },
    catch: { syntax: ['catch (e)\n    ...', 'catch e\n    ...'], doc: 'Maneja una excepcion del try de arriba, ligandola a e. Los parentesis son opcionales.' },
    finally: { syntax: ['try\n    ...\nfinally\n    ...\nend'], doc: 'Corre despues del try, se haya lanzado una excepcion o no.' },
    select: { syntax: ['select expr\n    case v1, v2 then\n        ...\n    case a to b then\n        ...\n    case is >= v then\n        ...\n    else\n        ...\nend'],
        doc: 'Compara expr contra cada case en orden y corre el primero que matchee. Cierra con un solo end (sin end select, sin case else -- usa else).' },
    case: { syntax: ['case item, item then ...'], doc: 'Una rama de select. Cada item puede ser un valor (igualdad), "a to b" (rango inclusivo), o "is op valor" (relacional).' },
    to: { syntax: ['case a to b then ...'], doc: "Dentro de un case, matchea cuando el valor de select cae entre a y b (inclusive)." },
    is: { syntax: ['case is compOp valor then ...'], doc: 'Dentro de un case, compara el valor de select contra valor usando compOp (==, !=, <, <=, >, >=).' },
    static: { syntax: ['static func nombre(...) ... end', 'static atributo = valor'],
        doc: 'Marca un miembro de clase como estatico (pertenece a la clase, no a una instancia). Combinable con private.' },
    private: { syntax: ['private func nombre(...) ... end', 'private atributo = valor'],
        doc: 'Marca un miembro de clase como privado, restringiendo el acceso desde fuera de la clase. Combinable con static.' },
    async: { syntax: ['async func nombre(...) ... end'], doc: 'Declara una funcion asincrona. Su cuerpo puede usar await.' },
    await: { syntax: ['await expr'], doc: 'Suspende la funcion async actual hasta que expr (tipicamente otra funcion async) termine.' },
    extern: { syntax: ['extern "lib" as Alias\n    func Nombre(...)\n    ...\nend'],
        doc: 'Declara funciones implementadas por una libreria nativa, resuelta en runtime contra el alias dado. Solo declaraciones, sin cuerpo.' },
    or: { syntax: ['a or b'], doc: 'OR logico: verdadero si cualquiera de los dos lados es verdadero (con corto-circuito).' },
    and: { syntax: ['a and b'], doc: 'AND logico: verdadero solo si ambos lados son verdaderos (con corto-circuito).' },
    not: { syntax: ['not a'], doc: 'Negacion logica.' },
};

const BUILTIN_SIGNATURES = {
    type: { params: ['value'], doc: 'Devuelve el nombre de tipo de value como string ("nil", "bool", "number", "string", "list", "dict", "function", "instance", "class", "coroutine", "native", "bound" o "exception").' },
    str: { params: ['value'], doc: 'Convierte value a su string de despliegue (el mismo formato que usa print()).' },
    int: { params: ['value'], doc: 'Trunca value hacia cero y lo devuelve como numero.' },
    float: { params: ['value'], doc: 'Convierte value a un numero de punto flotante.' },
    print: { params: ['*values'], doc: 'Imprime cada argumento separado por espacios, seguido de un salto de linea.' },
    input: { params: ['prompt?'], doc: 'Lee una linea desde la entrada estandar, mostrando prompt si se da.' },
    abs: { params: ['numero'], doc: 'Valor absoluto de numero.' },
    round: { params: ['numero'], doc: 'Redondea numero al entero mas cercano.' },
    floor: { params: ['numero'], doc: 'Redondea numero hacia abajo.' },
    ceil: { params: ['numero'], doc: 'Redondea numero hacia arriba.' },
    min: { params: ['*values'], doc: 'El valor mas chico: min(a, b, ...) o min(lista).' },
    max: { params: ['*values'], doc: 'El valor mas grande: max(a, b, ...) o max(lista).' },
    pow: { params: ['base', 'exponente'], doc: 'base elevado a exponente.' },
    sqrt: { params: ['numero'], doc: 'Raiz cuadrada de numero.' },
    sum: { params: ['*values'], doc: 'Suma numerica: sum(a, b, ...) o sum(lista).' },
    sorted: { params: ['lista'], doc: 'Nueva lista con los items de lista ordenados ascendente.' },
    reversed: { params: ['lista'], doc: 'Nueva lista con los items de lista en orden inverso.' },
    any: { params: ['*values'], doc: 'true si al menos un valor/item es verdadero (truthy).' },
    all: { params: ['*values'], doc: 'true solo si todos los valores/items son verdaderos (truthy).' },
    len: { params: ['value'], doc: 'Largo de un string, lista o dict.' },
    range: { params: ['fin'], doc: 'Lista de numeros. Formas: range(fin), range(inicio, fin), range(inicio, fin, paso).' },
    slice: { params: ['value', 'inicio', 'fin?'], doc: 'Sub-lista o sub-string entre inicio y fin.' },
    setglobal: { params: ['nombre', 'valor'], doc: 'Define o reasigna una variable global por nombre.' },
    mem_is_null: { params: ['ptr'], doc: 'true si ptr (de una llamada extern) es nulo.' },
    mem_peek_string: { params: ['ptr'], doc: 'Lee un string desde un puntero nativo (uso con extern/FFI).' },
    mem_peek_ptr: { params: ['ptr', 'offset?'], doc: 'Lee un puntero nativo desde otro puntero (uso con extern/FFI).' },
    coroutine: { params: ['func'], doc: 'Crea una corrutina a partir de una funcion.' },
    resume: { params: ['coroutine', '*args'], doc: 'Reanuda una corrutina pausada.' },
    set_timeout: { params: ['func', 'ms'], doc: 'Agenda func para correr despues de ms milisegundos.' },
    sleep_async: { params: ['ms'], doc: 'Espera ms milisegundos sin bloquear (usar con await en un func async).' },
    clear_timeout: { params: ['handle'], doc: 'Cancela un timeout agendado con set_timeout.' },
    delay: { params: ['ms'], doc: 'Espera ms milisegundos (version sincrona/bloqueante).' },
};

const PLAIN_KEYWORDS = [
    'if', 'then', 'elif', 'else', 'end', 'while', 'for', 'in', 'func', 'class',
    'base', 'this', 'self', 'return', 'break', 'continue', 'pass', 'import',
    'as', 'local', 'raise', 'try', 'catch', 'finally', 'select', 'case', 'to',
    'is', 'static', 'private', 'async', 'await', 'extern', 'or', 'and', 'not',
    'true', 'false', 'nil', 'new',
];

const RE_FUNC = /^[ \t]*(?:(static)\s+)?(?:(private)\s+)?(?:(async)\s+)?func\s+([A-Za-z_]\w*)\s*\(([^)]*)\)\s*(?:as\s+([A-Za-z_]\w*))?/;
const RE_CLASS = /^[ \t]*class\s+([A-Za-z_]\w*)\s*(?::\s*([A-Za-z_]\w*))?/;
const RE_ATTR = /^[ \t]*(?:(static)\s+)?(?:(private)\s+)?([A-Za-z_]\w*)\s*(?:as\s+([A-Za-z_]\w*))?\s*=\s*(.*)$/;
const RE_ATTR_INIT_CLASS = /^(?:new\s+)?([A-Za-z_]\w*)\s*\(/;
const RE_IMPORT = /^[ \t]*import\s+([A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*)(?:\s+as\s+([A-Za-z_]\w*))?/;
const RE_BLOCK_OPEN = /^[ \t]*(?:(?:static|private|async)\s+)*(if|while|for|func|class|try|select|extern)\b/;
const RE_BLOCK_END = /^[ \t]*end\b/;
const RE_CLAUSE = /^[ \t]*(elif|else|catch|finally|case)\b/;
const CLAUSE_DEPTH_OFFSETS = { select: 1 };

function buildVarAssignRegex(varName) {
    const escaped = varName.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
    return new RegExp(
        '(?:^|[\\n;])[ \\t]*(?:local\\s+)?' + escaped +
        '\\s*(?:as\\s+[A-Za-z_]\\w*\\s*)?=\\s*(?:new\\s+)?([A-Za-z_]\\w*)\\s*\\(',
        'g'
    );
}

function parseParams(raw) {
    return raw.split(',').map((p) => p.trim()).filter(Boolean);
}

function inferReturnType(lines, startLine, endLine, classesMap) {
    const localVarClass = new Map();
    const assignRe = /^[ \t]*(?:local\s+)?([A-Za-z_]\w*)\s*(?:as\s+[A-Za-z_]\w*\s*)?=\s*(?:new\s+)?([A-Za-z_]\w*)\s*\(/;
    const returnNewRe = /^[ \t]*return\s+new\s+([A-Za-z_]\w*)\s*\(/;
    const returnVarRe = /^[ \t]*return\s+([A-Za-z_]\w*)\s*$/;
    const last = Math.min(endLine, lines.length - 1);
    for (let i = Math.max(startLine, 0); i <= last; ++i) {
        const line = lines[i];
        const assignMatch = assignRe.exec(line);
        if (assignMatch && classesMap.has(assignMatch[2])) {
            localVarClass.set(assignMatch[1], assignMatch[2]);
        }
        const retNew = returnNewRe.exec(line);
        if (retNew && classesMap.has(retNew[1])) return retNew[1];
        const retVar = returnVarRe.exec(line);
        if (retVar && localVarClass.has(retVar[1])) return localVarClass.get(retVar[1]);
    }
    return null;
}

function splitChainString(str) {
    const segments = [];
    let buf = '';
    let depth = 0;
    for (const ch of str) {
        if (ch === '(') depth++;
        if (ch === ')') depth--;
        if (ch === '.' && depth === 0) {
            segments.push(buf);
            buf = '';
            continue;
        }
        buf += ch;
    }
    segments.push(buf);
    return segments;
}

function extractChainSegments(text, dotOffset) {
    let start = dotOffset;
    let depth = 0;
    while (start > 0) {
        const ch = text[start - 1];
        if (ch === ')') { depth++; start--; continue; }
        if (ch === '(') {
            if (depth === 0) break;
            depth--; start--; continue;
        }
        if (depth > 0) { start--; continue; }
        if (/[A-Za-z0-9_.]/.test(ch)) { start--; continue; }
        break;
    }
    const chainText = text.slice(start, dotOffset);
    if (!chainText) return null;
    const segments = splitChainString(chainText);
    for (const seg of segments) {
        if (!/^[A-Za-z_]\w*(\([\s\S]*\))?$/.test(seg)) return null;
    }
    return segments;
}

function parseFileText(text) {
    const lines = text.split(/\r\n|\r|\n/);
    const functions = new Map();
    const classes = new Map();  
    const imports = [];       

    const stack = [];

    for (let i = 0; i < lines.length; ++i) {
        const line = lines[i];
        const indent = line.length - line.replace(/^[ \t]*/, '').length;

        if (RE_BLOCK_END.test(line)) {
            while (stack.length && stack[stack.length - 1].indent >= indent) {
                const popped = stack.pop();
                if (popped.info && (popped.type === 'func' || popped.type === 'class')) {
                    popped.info.endLine = i;
                }
            }
            continue;
        }

        const importMatch = RE_IMPORT.exec(line);
        if (importMatch) {
            imports.push({ parts: importMatch[1].split('.'), alias: importMatch[2] || null, line: i });
        }

        const classMatch = RE_CLASS.exec(line);
        if (classMatch) {
            const info = { base: classMatch[2] || null, methods: new Map(), attributes: new Map(), line: i };
            classes.set(classMatch[1], info);
            stack.push({ type: 'class', name: classMatch[1], indent, info });
            continue;
        }

        const funcMatch = RE_FUNC.exec(line);
        if (funcMatch) {
            const [, isStatic, isPrivate, isAsync, name, rawParams, returnType] = funcMatch;
            const enclosing = stack.length && stack[stack.length - 1].type === 'class' ? stack[stack.length - 1] : null;
            const entry = {
                params: parseParams(rawParams), isAsync: !!isAsync, isStatic: !!isStatic, isPrivate: !!isPrivate,
                line: i, returnType: returnType || null, returnTypeExplicit: !!returnType, endLine: null,
            };
            if (enclosing) {
                enclosing.info.methods.set(name, entry);
            } else {
                functions.set(name, entry);
            }
            stack.push({ type: 'func', name, indent, info: entry });
            continue;
        }

        const blockMatch = RE_BLOCK_OPEN.exec(line);
        if (blockMatch && blockMatch[1] !== 'func' && blockMatch[1] !== 'class') {
            stack.push({ type: blockMatch[1], indent });
            continue;
        }

        if (stack.length && stack[stack.length - 1].type === 'class') {
            const attrMatch = RE_ATTR.exec(line);
            if (attrMatch) {
                const [, isStatic, isPrivate, name, annotatedType, rhs] = attrMatch;
                let type = annotatedType || null;
                if (!type && rhs) {
                    const initMatch = RE_ATTR_INIT_CLASS.exec(rhs.trim());
                    if (initMatch) type = initMatch[1];
                }
                stack[stack.length - 1].info.attributes.set(name, { isStatic: !!isStatic, isPrivate: !!isPrivate, line: i, type });
            }
        }
    }

    for (const [, entry] of functions) {
        if (!entry.returnTypeExplicit && entry.endLine != null) {
            entry.returnType = inferReturnType(lines, entry.line + 1, entry.endLine - 1, classes);
        }
    }
    for (const [, cls] of classes) {
        for (const [, entry] of cls.methods) {
            if (!entry.returnTypeExplicit && entry.endLine != null) {
                entry.returnType = inferReturnType(lines, entry.line + 1, entry.endLine - 1, classes);
            }
        }
    }

    return { functions, classes, imports, lineCount: lines.length };
}

function enclosingClassAt(text, line) {
    const lines = text.split(/\r\n|\r|\n/);
    const stack = [];
    let result = null;
    for (let i = 0; i <= Math.min(line, lines.length - 1); ++i) {
        const l = lines[i];
        const indent = l.length - l.replace(/^[ \t]*/, '').length;
        if (RE_BLOCK_END.test(l)) {
            while (stack.length && stack[stack.length - 1].indent >= indent) stack.pop();
            continue;
        }
        const classMatch = RE_CLASS.exec(l);
        if (classMatch) {
            stack.push({ type: 'class', name: classMatch[1], indent });
            continue;
        }
        const blockMatch = RE_BLOCK_OPEN.exec(l);
        if (blockMatch) {
            stack.push({ type: blockMatch[1], indent });
        }
    }
    for (let i = stack.length - 1; i >= 0; --i) {
        if (stack[i].type === 'class') { result = stack[i].name; break; }
    }
    return result;
}

function stripLineComment(line) {
    let inString = null;
    for (let i = 0; i < line.length; ++i) {
        const ch = line[i];
        if (inString) {
            if (ch === '\\') { i++; continue; }
            if (ch === inString) inString = null;
            continue;
        }
        if (ch === '"' || ch === "'") { inString = ch; continue; }
        if (ch === '#') return line.slice(0, i);
    }
    return line;
}

const RE_NEEDS_THEN = /^[ \t]*(?:if|elif|for|case)\b/;

function findMissingThen(text) {
    const lines = text.split(/\r\n|\r|\n/);
    const issues = [];
    for (let i = 0; i < lines.length; ++i) {
        const raw = lines[i];
        if (!RE_NEEDS_THEN.test(raw)) continue;
        const stripped = stripLineComment(raw).replace(/\s+$/, '');
        if (!stripped.trim()) continue;
        if (/\bthen$/.test(stripped)) continue;
        issues.push({ line: i, column: stripped.length });
    }
    return issues;
}

function analyzeBlocks(text) {
    const lines = text.split(/\r\n|\r|\n/);
    const stack = [];
    const unmatchedEnds = [];
    for (let i = 0; i < lines.length; ++i) {
        const line = lines[i];
        const indent = line.length - line.replace(/^[ \t]*/, '').length;
        if (RE_BLOCK_END.test(line)) {
            if (!stack.length) {
                unmatchedEnds.push({ line: i });
                continue;
            }
            while (stack.length && stack[stack.length - 1].indent >= indent) stack.pop();
            continue;
        }
        const classMatch = RE_CLASS.exec(line);
        if (classMatch) { stack.push({ line: i, indent, keyword: 'class' }); continue; }
        const funcMatch = RE_FUNC.exec(line);
        if (funcMatch) { stack.push({ line: i, indent, keyword: 'func' }); continue; }
        const blockMatch = RE_BLOCK_OPEN.exec(line);
        if (blockMatch && blockMatch[1] !== 'func' && blockMatch[1] !== 'class') {
            stack.push({ line: i, indent, keyword: blockMatch[1] });
        }
    }
    return { unclosed: stack, unmatchedEnds };
}

function directEnclosingBlockAt(text, line) {
    const lines = text.split(/\r\n|\r|\n/);
    const stack = [];
    for (let i = 0; i < line && i < lines.length; ++i) {
        const l = lines[i];
        const indent = l.length - l.replace(/^[ \t]*/, '').length;
        if (RE_BLOCK_END.test(l)) {
            while (stack.length && stack[stack.length - 1].indent >= indent) stack.pop();
            continue;
        }
        const classMatch = RE_CLASS.exec(l);
        if (classMatch) { stack.push({ type: 'class', name: classMatch[1], indent }); continue; }
        const funcMatch = RE_FUNC.exec(l);
        if (funcMatch) { stack.push({ type: 'func', name: funcMatch[4], indent }); continue; }
        const blockMatch = RE_BLOCK_OPEN.exec(l);
        if (blockMatch) stack.push({ type: blockMatch[1], indent });
    }
    return stack.length ? stack[stack.length - 1] : null;
}

function classifyBlockLine(line) {
    if (RE_BLOCK_END.test(line)) return { kind: 'end', keyword: 'end' };
    const classMatch = RE_CLASS.exec(line);
    if (classMatch) return { kind: 'open', keyword: 'class' };
    const funcMatch = RE_FUNC.exec(line);
    if (funcMatch) return { kind: 'open', keyword: 'func' };
    const clauseMatch = RE_CLAUSE.exec(line);
    if (clauseMatch) return { kind: 'clause', keyword: clauseMatch[1] };
    const blockMatch = RE_BLOCK_OPEN.exec(line);
    if (blockMatch && blockMatch[1] !== 'func' && blockMatch[1] !== 'class') {
        return { kind: 'open', keyword: blockMatch[1] };
    }
    return { kind: 'normal', keyword: null };
}

function levenshtein(a, b) {
    const rows = a.length + 1;
    const cols = b.length + 1;
    const dist = Array.from({ length: rows }, (_, i) => [i, ...new Array(cols - 1).fill(0)]);
    for (let j = 0; j < cols; ++j) dist[0][j] = j;
    for (let i = 1; i < rows; ++i) {
        for (let j = 1; j < cols; ++j) {
            const cost = a[i - 1] === b[j - 1] ? 0 : 1;
            dist[i][j] = Math.min(dist[i - 1][j] + 1, dist[i][j - 1] + 1, dist[i - 1][j - 1] + cost);
        }
    }
    return dist[rows - 1][cols - 1];
}

function closestNames(target, candidates, maxDistance, maxResults) {
    const scored = [];
    for (const candidate of candidates) {
        if (candidate === target) continue;
        const distance = levenshtein(target, candidate);
        if (distance <= maxDistance) scored.push({ candidate, distance });
    }
    scored.sort((a, b) => a.distance - b.distance || a.candidate.localeCompare(b.candidate));
    const seen = new Set();
    const result = [];
    for (const entry of scored) {
        if (seen.has(entry.candidate)) continue;
        seen.add(entry.candidate);
        result.push(entry.candidate);
        if (result.length >= maxResults) break;
    }
    return result;
}

function parseAttributeLine(line) {
    const match = RE_ATTR.exec(line);
    if (!match) return null;
    const [, isStatic, isPrivate, name, annotatedType, rhs] = match;
    let type = annotatedType || null;
    if (!type && rhs) {
        const initMatch = RE_ATTR_INIT_CLASS.exec(rhs.trim());
        if (initMatch) type = initMatch[1];
    }
    return { name, type, typeExplicit: !!annotatedType, isStatic: !!isStatic, isPrivate: !!isPrivate };
}

function funcDeclarationSpan(line) {
    const match = RE_FUNC.exec(line);
    if (!match) return null;
    const name = match[4];
    const nameIndex = line.indexOf(name, match.index);
    const openParenIndex = line.indexOf('(', nameIndex);
    if (openParenIndex === -1) return null;
    let depth = 0;
    let closeIndex = -1;
    for (let i = openParenIndex; i < line.length; ++i) {
        if (line[i] === '(') depth++;
        else if (line[i] === ')') {
            depth--;
            if (depth === 0) { closeIndex = i; break; }
        }
    }
    if (closeIndex === -1) return null;
    return { nameIndex, openParenIndex, closeParenIndex: closeIndex };
}

function collectBlockRanges(text) {
    const lines = text.split(/\r\n|\r|\n/);
    const stack = [];
    const ranges = [];
    for (let i = 0; i < lines.length; ++i) {
        const line = lines[i];
        const indent = line.length - line.replace(/^[ \t]*/, '').length;
        if (RE_BLOCK_END.test(line)) {
            while (stack.length && stack[stack.length - 1].indent >= indent) {
                const popped = stack.pop();
                if (i > popped.line) ranges.push({ startLine: popped.line, endLine: i });
            }
            continue;
        }
        const classMatch = RE_CLASS.exec(line);
        if (classMatch) {
            stack.push({ line: i, indent });
            continue;
        }
        const funcMatch = RE_FUNC.exec(line);
        if (funcMatch) {
            stack.push({ line: i, indent });
            continue;
        }
        const blockMatch = RE_BLOCK_OPEN.exec(line);
        if (blockMatch && blockMatch[1] !== 'func' && blockMatch[1] !== 'class') {
            stack.push({ line: i, indent });
        }
    }
    return ranges;
}

const fileCache = new Map();

function readAndParse(filePath) {
    let stat;
    try { stat = fs.statSync(filePath); } catch (e) { return null; }
    const cached = fileCache.get(filePath);
    if (cached && cached.mtimeMs === stat.mtimeMs) return cached.parsed;
    let text;
    try { text = fs.readFileSync(filePath, 'utf8'); } catch (e) { return null; }
    const parsed = parseFileText(text);
    parsed.text = text;
    parsed.path = filePath;
    fileCache.set(filePath, { mtimeMs: stat.mtimeMs, text, parsed });
    return parsed;
}

function candidatePaths(moduleParts, currentDir, modulesPath, workspaceRoot) {
    const rel = moduleParts.join(path.sep);
    const candidates = [];
    if (currentDir) {
        candidates.push(path.join(currentDir, rel + '.ava'));
        candidates.push(path.join(currentDir, rel, 'index.ava'));
    }
    if (modulesPath) {
        candidates.push(path.join(modulesPath, rel, 'index.ava'));
        candidates.push(path.join(modulesPath, rel + '.ava'));
    }
    if (workspaceRoot) {
        candidates.push(path.join(workspaceRoot, rel + '.ava'));
        candidates.push(path.join(workspaceRoot, rel, 'index.ava'));
    }
    return candidates;
}

function resolveModuleFile(moduleParts, currentDir, modulesPath, workspaceRoot) {
    for (const candidate of candidatePaths(moduleParts, currentDir, modulesPath, workspaceRoot)) {
        if (fs.existsSync(candidate)) return candidate;
    }
    return null;
}

function buildIndex(filePath, text, modulesPath, workspaceRoot) {
    const functions = new Map();
    const classes = new Map();
    const importedModules = new Map(); 
    const visited = new Set();

    function mergeInto(targetFns, targetClasses, parsed) {
        for (const [name, info] of parsed.functions) if (!targetFns.has(name)) targetFns.set(name, Object.assign({}, info, { file: parsed.path }));
        for (const [name, info] of parsed.classes) if (!targetClasses.has(name)) targetClasses.set(name, Object.assign({}, info, { file: parsed.path }));
    }

    function visit(currentPath, currentText, depth) {
        if (visited.has(currentPath) || depth > 6) return;
        visited.add(currentPath);
        const parsed = currentText != null
            ? Object.assign(parseFileText(currentText), { path: currentPath, text: currentText })
            : readAndParse(currentPath);
        if (!parsed) return;

        if (currentPath === filePath) {
            mergeInto(functions, classes, parsed);
        }

        for (const imp of parsed.imports) {
            const currentDir = path.dirname(currentPath);
            const resolved = resolveModuleFile(imp.parts, currentDir, modulesPath, workspaceRoot);
            if (!resolved) continue;
            const importedParsed = readAndParse(resolved);
            if (!importedParsed) continue;

            if (currentPath === filePath) {
                const key = imp.alias || imp.parts[imp.parts.length - 1];
                const wrappedFunctions = new Map();
                for (const [name, info] of importedParsed.functions) wrappedFunctions.set(name, Object.assign({}, info, { file: resolved }));
                const wrappedClasses = new Map();
                for (const [name, info] of importedParsed.classes) wrappedClasses.set(name, Object.assign({}, info, { file: resolved }));
                importedModules.set(key, {
                    functions: wrappedFunctions,
                    classes: wrappedClasses,
                    path: resolved,
                });

                for (const [name, info] of wrappedClasses) {
                    if (!classes.has(name)) classes.set(name, info);
                }
            }
            visit(resolved, null, depth + 1);
        }
    }

    visit(filePath, text, 0);
    return { functions, classes, importedModules };
}

function listAvailableModules(modulesPath, workspaceRoot) {
    const names = new Set();
    const scan = (root) => {
        if (!root) return;
        let entries;
        try { entries = fs.readdirSync(root, { withFileTypes: true }); } catch (e) { return; }
        for (const entry of entries) {
            if (entry.isDirectory() && fs.existsSync(path.join(root, entry.name, 'index.ava'))) {
                names.add(entry.name);
            } else if (entry.isFile() && entry.name.endsWith('.ava')) {
                names.add(entry.name.replace(/\.ava$/, ''));
            }
        }
    };
    scan(modulesPath);
    scan(workspaceRoot);
    return Array.from(names).sort();
}

module.exports = {
    KEYWORD_DOCS,
    BUILTIN_SIGNATURES,
    PLAIN_KEYWORDS,
    parseFileText,
    enclosingClassAt,
    buildIndex,
    resolveModuleFile,
    listAvailableModules,
    buildVarAssignRegex,
    splitChainString,
    extractChainSegments,
    collectBlockRanges,
    findMissingThen,
    analyzeBlocks,
    directEnclosingBlockAt,
    parseAttributeLine,
    levenshtein,
    closestNames,
    funcDeclarationSpan,
    classifyBlockLine,
    CLAUSE_DEPTH_OFFSETS,
};
