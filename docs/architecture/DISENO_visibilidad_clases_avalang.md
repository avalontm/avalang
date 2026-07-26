# Diseño — Visibilidad de miembros de clase (`static` / `private`) en AvaLang

> **Nota sobre este documento**: se reconstruyó a partir del código ya
> implementado (grammar, AST, compilador, VM y Ava Studio lo referencian por
> nombre y número de sección: `grammar/AvaLang.g4`, `core/src/ast/ast.h`,
> `core/src/compiler/compiler.cpp`, `core/src/vm/value.h`, `core/src/vm/vm.cpp`,
> `studio/src/languages/class_index.h/.cpp`, `scripts/visibilidad_modificadores.ava`)
> pero el archivo original no estaba presente en el repo/zip. La numeración de
> secciones y fases de abajo es la que ya citan esos comentarios; el texto es
> una descripción fiel de lo que el código hace, no un documento de diseño
> previo a la implementación. Si en algún momento aparece el original, esta
> versión debería reemplazarse o fusionarse con él.

## §1 — Motivación

Antes de este trabajo, un atributo de clase declarado en el cuerpo (`x = 0`
dentro de `class Foo ... end`) vivía siempre en `ClassObj::attrs`, sin
distinguir "compartido por todas las instancias" de "valor por defecto que
cada instancia copia la suya". Eso producía un bug real: `Clase.x = valor`
nunca quedaba realmente compartido entre instancias ya creadas, porque cada
`CALL`/`NEWINSTANCE` sobre la clase copiaba `attrs` completo al `InstanceObj`
nuevo, así que cada instancia terminaba con su propia copia independiente —
lo opuesto a lo que "compartido" debería significar.

La solución fue introducir dos modificadores de miembro, `static` y
`private`, y separar de una vez el almacenamiento de atributos compartidos
(`ClassObj::attrs`) de los valores por defecto de instancia
(`ClassObj::instance_defaults`) — ver Fase D.

## §3 — Semántica

| Modificador(es)     | Almacenamiento                          | Visibilidad                          |
|----------------------|------------------------------------------|---------------------------------------|
| (ninguno)             | por instancia (`instance_defaults`)      | pública (comportamiento de siempre)   |
| `private`             | por instancia (`instance_defaults`)      | NO pública                            |
| `static`              | compartido (`attrs`, en la clase)        | pública                               |
| `static private`      | compartido (`attrs`, en la clase)        | NO pública                            |

### §3.1 — Métodos `static`

Un método `static func nombre(...)` no recibe instancia: no se registra
`this` como local dentro de él. Por lo tanto `this.algo` adentro de un
método `static` no resuelve a un atributo — cae a búsqueda de variable
global, igual que cualquier `this` fuera de una clase — y el azúcar de
"nombre pelado resuelve a `this.attr`" (`instance_attrs_` en el compilador)
tampoco aplica ahí. Un método `static` se llama siempre explícitamente,
`NombreDeClase.metodo(...)`.

### §3.3 — Herencia y `private`

Los atributos/métodos `private` de una clase base SÍ siguen siendo
utilizables desde dentro de los propios métodos heredados (porque esos
métodos ya fueron compilados en el contexto de la clase base, con acceso
directo a `this.eso`). Lo que NO pasa es que una clase hija "herede" el
privilegio de ver ese miembro desde fuera, ni siquiera desde sus propios
métodos: un miembro `private` inherited nunca es "propio" (`is_own_private_context`
en `ClassIndex::FilterForAccess`, ver §9) para la subclase, solo para la
clase que lo declaró.

Un `this.NAME = ...` dentro de un método es, para el escaneo best-effort del
editor (`ClassIndex`), evidencia de un atributo de instancia — pero no le
asigna visibilidad por sí solo: la visibilidad real de un atributo la fija
su declaración explícita en el cuerpo de la clase (`private NAME = ...`), si
existe una.

## §4/§5 — Modificadores y reglas de acceso

`memberModifier` es `static` y/o `private`, en cualquier orden (`static
private x` y `private static x` son ambos válidos — ver Fase A). Se aplican
tanto a declaraciones de atributo (`modifiedAssignStatement`) como de método
(`modifiedFuncDeclaration`).

Reglas de acceso (aplicadas por `ClassIndex::FilterForAccess`, y reflejadas
en el enforcement real del compilador donde corresponde):

- **`variable.`** (una instancia, desde afuera de la clase) → solo miembros
  públicos, sean `static` o no.
- **`this.`** (dentro de un método de la clase `viewer_class`) → todo
  miembro público, más los miembros `private` propios de `viewer_class`
  (nunca uno `private` heredado de una clase base — ver §3.3).
- **`NombreDeClase.`** (acceso directo al objeto de clase) → solo miembros
  `static`. Un `static private` solo se ofrece desde dentro de su propia
  clase declarante (p. ej. `Contador.validarLimite` llamado desde otro
  método `static` de `Contador`); un `static` público siempre se ofrece.

## §6 — El VM no aplica `private` como control de acceso real

`ClassObj::private_members` es solo metadata (qué nombres se declararon
`private`). En esta fase, **el VM no la usa para negar acceso en runtime** —
nada le impide a un script llamar `instancia.metodo_privado()` si conoce el
nombre. El enforcement real de `private` ocurre del lado de Ava Studio (Fase
E/F): el autocompletado no lo sugiere, pero el lenguaje en sí no lo prohíbe.
Es, por ahora, una convención de visibilidad de herramientas/editor, no una
garantía del lenguaje.

## §9 — Tabla de las tres reglas de acceso

| `kind` (`MemberAccessKind`) | Contexto                         | Público | Privado propio (`declared_in == viewer_class`) | Privado heredado |
|-------------------------------|-----------------------------------|:-------:|:--------------------------------------------------:|:--------------------:|
| `kInstance`                    | `variable.` desde afuera          |   sí    |                         no                          |          no           |
| `kThis`                         | `this.` dentro de un método       |   sí    |                         sí                          |          no           |
| `kClassName`                    | `NombreDeClase.` (solo `static`)  |   sí    |                         sí                          |          no           |

## §10 — Plan de pruebas

`scripts/visibilidad_modificadores.ava` cubre las 4 combinaciones de
modificadores (ninguno / `private` / `static` / `static private`) sobre
atributos y métodos, con una jerarquía `Animal` → `Dog` y una clase
`Contador` puramente `static`, ejercitando:

- Atributo público de instancia, visible y usable desde afuera.
- Atributo `private` de instancia, usado internamente (`this.vidaSecreta`
  dentro de otro método `private` de la misma clase).
- Atributo `static` público (`Animal.totalAnimales`), compartido de verdad
  entre instancias (incrementado en el constructor, leído después de crear
  varias).
- Método `static private` (`Contador.validarLimite`), llamado desde otro
  método `static` de la misma clase (`Contador.crear`) sin problema, pero
  no ofrecido en autocompletado desde afuera.
- Herencia: `Dog : Animal` ve los atributos/métodos públicos heredados
  (`nombre`, `comer`, `especie`), pero no los `private` de `Animal`
  (`vidaSecreta`, `regenerar`).

`studio/tests/member_access_test.cpp` (standalone, ver su comentario de
cabecera para el comando de compilación) ejercita `ClassIndex`,
`VariableTypeIndex` y `ResolveMemberAccess` contra este script y contra
`scripts/dog.ava`, incluyendo específicamente los casos de la tabla de §9.

## Fases de implementación

- **Fase A** — Gramática (`grammar/AvaLang.g4`): reglas `memberModifier`,
  `modifiedAssignStatement`, `modifiedFuncDeclaration`. Deliberadamente
  aditivas: no tocan `block`/`classDeclaration`/`statement`, así que
  sintácticamente `static`/`private` quedan válidos en cualquier lugar
  donde ya valía un statement — la restricción real (solo tienen sentido
  dentro de un cuerpo de clase) se valida después, en el compilador
  (Fase C), no en la gramática. Esto evita reescribir reglas reutilizadas
  por funciones/if/while/for y mantiene en cero el riesgo de romper algo
  existente.
- **Fase B** — AST (`core/src/ast/ast.h`, `ast_builder.cpp`): `AssignStmt` y
  `FuncDef` ganan `is_static`/`is_private`; `ast_builder.cpp` resuelve
  `memberModifier+` a esos dos flags. Mismo caveat que en la gramática:
  significan algo real solo cuando el nodo vive dentro de un `ClassDef`.
- **Fase C** — Compilador (`core/src/compiler/compiler.cpp`):
  `RejectMemberModifiersOutsideClass` rechaza `static`/`private` fuera de
  un cuerpo de clase. Dentro de `CompileClass`, separa atributos `static`
  (a `class_obj->attrs`) de atributos de instancia (a
  `class_obj->instance_defaults`) — antes de esta fase ambos caían en
  `attrs`, la causa raíz del bug de §1. También filtra qué nombres entran
  a `instance_attrs_` (solo defaults de instancia, nunca `static`) para
  que el azúcar de nombre-pelado-resuelve-a-`this.attr` no aplique a
  estáticos.
- **Fase D** — VM (`core/src/vm/value.h`, `vm.cpp`): `ClassObj` separa
  `attrs` (compartido) de `instance_defaults` (por instancia).
  `NEWINSTANCE` y el `CALL` sobre un valor `Class` copian solo
  `instance_defaults` al `InstanceObj` nuevo, nunca `attrs` completo —
  el fix real del bug de §1. `GETATTR`/`SETATTR` sobre un valor `Class`
  recorren la cadena `__base__` (`FindClassOwningAttr`) para encontrar
  qué clase de la jerarquía es dueña de un atributo `static` dado, ya
  que estos no se copian a las subclases (viven solo en la clase que
  los declaró).
- **Fase E** — Ava Studio, escaneo (`studio/src/languages/class_index.cpp`):
  `ClassIndex::ScanText` reconoce el prefijo `static`/`private` (en
  cualquier orden) antes de una declaración de atributo o método, y lo
  guarda en `ClassMethodInfo::is_static/is_private` /
  `ClassAttributeInfo::is_static/is_private`.
- **Fase F** — Ava Studio, filtrado + pruebas: `ClassIndex::FilterForAccess`
  aplica la tabla de §9 sobre una lista de `ClassMember` ya aplanada
  (`FlattenedMembers`), y `PopulateMemberSuggestions`
  (`editor_panel.cpp`) la llama antes de poblar el popup de
  autocompletado. `scripts/visibilidad_modificadores.ava` +
  `member_access_test.cpp` son la prueba end-to-end de todo lo anterior
  (ver §10).
