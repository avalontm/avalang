# avapack — empacador de proyectos AvaLang en un solo ejecutable

Este componente implementa `ava_cli build`: toma un proyecto AvaLang (`.ava`/`.avaui` +
imports) y genera un único `.exe` que lo ejecuta sin dejar la carpeta del proyecto visible al
lado del binario.

Plan completo de fases: `plan_ava_pack.md` (raíz del repo, fuera de control de versiones o en
`docs/`, según se decida). Este README documenta las decisiones que salieron de la **Fase 0**
y sirve de referencia para no reabrirlas en fases posteriores.

---

## Decisiones de Fase 0

### 1. Layout del componente

`avapack` vive **enteramente en C++**, no en Python:

- `runtime/avapack/` — el generador (`avapack_gen`, produce `embedded_project.cpp` a partir de
  un directorio de proyecto) y la plantilla de runtime empacado (`main.cpp`, se compila junto
  a los archivos generados para producir el `.exe` final).
- `runtime/avapack/third_party/` — librerías vendorizadas que solo necesita el empacador (ver
  punto 5). No vive bajo `runtime/avalang/third_party/` a propósito: el core del VM no debe
  ganar una dependencia de cifrado que no usa.

**Por qué C++ y no `scripts/build/pack_project.py`:** el resto del pipeline de build de este
repo (`build.bat`, `build_cli.bat`, `build_studio.bat`, etc.) es CMake + batch puro, sin
dependencia de Python en ningún punto. Introducir Python como requisito para poder generar un
`.exe` habría sido la única dependencia externa nueva del pipeline. Se descarta esa opción del
plan original a favor de mantener todo en C++, compilado junto con `avalang`/`ava_cli` de la
forma habitual.

Esto también implica que el "generador" no es un script aparte que corre *antes* del build de
CMake — es un ejecutable propio (`avapack_gen`) que `ava_cli build` invoca como paso previo a
compilar el `.exe` final (se orquesta en Fase 2).

### 2. Qué se considera "código fuente del proyecto" a embeber

Fase 1 embebe **únicamente** `.ava` y `.avaui`. Quedan **fuera** del empacador por ahora:

- `appsettings.json` y cualquier config similar.
- Todo `wwwroot/` (CSS, iconos, assets estáticos servidos al navegador).
- `assets/` (fuentes, imágenes).

Razón: esos archivos son recursos de runtime que hoy ya resuelve `avahost` por su cuenta
(sirviéndolos como archivos estáticos), no código que el VM compile. Mezclarlos con el
embebido de código fuente complica el modelo de amenaza de las Fases 3-4 sin necesidad —
proteger un `.css` o un `.ttf` no tiene el mismo valor que proteger el código fuente `.ava`.
Si en el futuro se necesita empacar también esos assets (para un `.exe` que sirva un sitio sin
carpeta al lado), se trata como una fase nueva y separada, no como parte de este plan.

### 3. Contrato de línea de comandos

```
ava_cli build --project <dir> --entry <archivo.ava> --out <exe> [--key-file <path>] [--clean]
```

| Flag         | Obligatorio | Descripción                                                                 |
| ------------ | ----------- | ---------------------------------------------------------------------------- |
| `--project`  | sí          | Carpeta raíz del proyecto a empacar.                                        |
| `--entry`    | sí          | Archivo `.ava` de entrada, relativo a `--project`.                          |
| `--out`      | sí          | Ruta del `.exe` final.                                                      |
| `--key-file` | no          | Ruta a una clave AES externa (Fase 3+). Sin esta flag, la clave se genera aleatoriamente por build y se ofusca embebida (comportamiento por defecto). |
| `--clean`    | no          | Borra `build_pack/` antes de configurar, para un build totalmente fresco (recompila `avalang.dll` desde cero). Sin esta flag, `build_pack/` **persiste** entre corridas: CMake solo recompila lo que realmente cambió, así que `avalang.dll` no se recompila entero en cada `ava_cli build` (antes se borraba siempre al final, forzando una recompilación completa la próxima vez). |
| `--keep-temp`| no          | Deprecado — ya no hace nada distinto, `build_pack/` se conserva por defecto ahora. Se mantiene solo por compatibilidad. |

Nota: la configuración de CMake usada acá pasa `-DAVA_BUILD_UI=OFF` explícito, porque ni
`avapack_gen` ni el `.exe` empacado linkean `avalang_ui.dll` (ver `CMakeLists.txt` de esta
carpeta) — sin ese OFF, como `AVA_BUILD_UI` es `ON` por defecto en el `CMakeLists.txt` raíz, se
recompilaba `avalang_ui.dll` entera en cada build para nada. Si el proyecto empacado usa
componentes `.avaui` en runtime, la dll igual se copia junto al `.exe` final — pero se busca ya
compilada en `build_cli\runtime\avaui\<Config>\` (la deja ahí `build_cli.bat`), no se fuerza a
compilar de nuevo acá.

### 4. Dependencia de `avalang.dll`

Confirmado: el `.exe` empacado **sí** depende dinámicamente de `avalang.dll` (+ librerías
externas que ya use el proyecto). No se persigue un binario estático. Esto ya estaba decidido
antes de esta fase; queda documentado acá para que ninguna fase futura intente "arreglarlo"
por error.

### 5. Librería de cifrado (para Fase 3)

Se vendoriza **tiny-AES-c** (header-only, MIT) en `runtime/avapack/third_party/tiny-aes-c/`,
siguiendo el mismo patrón que `stb_truetype.h` en `runtime/avaui/third_party/stb/`. No se toca
en esta fase — la carpeta queda creada y vacía hasta Fase 3.

### 6. Estructura de carpetas

Ver `AVAPACK_STRUCT.md`.

---

## Modelo de amenaza (resumen, se detalla en Fase 3)

Igual que se documentará más a fondo en Fase 3: este empacador protege contra "abrir el `.exe`
en un editor de texto / correr `strings` y leer el código fuente". **No** protege contra un
atacante con debugger o volcado de memoria en tiempo de ejecución — la clave de descifrado
tiene que poder reconstruirse dentro del propio binario para que el programa funcione, así que
nunca puede ser criptográficamente inexpugnable contra ese nivel de atacante.

---

## Fase 3 — cifrado del contenido embebido

**Desvío respecto al plan, a revisar:** `plan_ava_pack.md` pedía específicamente AES-**GCM**.
La librería ya elegida en Fase 0 (`tiny-AES-c`) no incluye GCM — solo ECB/CBC/CTR. Cambié a
**AES-256-CTR** en vez de vendorizar una segunda librería de cifrado. El detalle completo del
razonamiento está en `third_party/tiny-aes-c/VENDOR.md`; resumen: GCM aporta autenticación
(detectar ciphertext modificado), no más confidencialidad que CTR, y la detección de
tampering del binario ya estaba prevista aparte en Fase 5. Avisame si esto no te cierra —
igual que con la decisión de Fase 0, más vale ajustarlo ahora que después de Fase 4.

### Qué se cifra y cómo

- Cada archivo `.ava`/`.avaui` se cifra individualmente con **AES-256-CTR**: una clave de 32
  bytes compartida por todo el build, y un **nonce/IV de 16 bytes aleatorio y distinto por
  archivo** (requisito real de seguridad de CTR — nunca se reusa un nonce con la misma clave
  dentro de un mismo build).
- La clave se genera al azar por build (`std::random_device`) salvo que se pase
  `--key-file <ruta>` (32 bytes crudos) a `avapack_gen` / `ava_cli build`.
- La clave **no** se guarda como 32 bytes contiguos en el binario: se parte en dos mitades de
  16 bytes, cada una XOReada contra una máscara calculada en tiempo de compilación
  (`avapack::KeyMaskByte`, función pura de un seed de 32 bits también aleatorio por build). El
  algoritmo de ofuscación en sí es público (vive en `embedded_project.h`); lo secreto por
  build es el seed + los fragmentos, generados de nuevo en cada build.
- `runtime/avapack/src/main.cpp` (la plantilla empacada) reconstruye la clave una sola vez al
  arrancar, descifra cada archivo en memoria justo antes de escribirlo al directorio temporal,
  y pone a cero tanto el plaintext como la copia local de la clave apenas terminan de usarse.

### Modelo de amenaza (detalle, resume lo ya dicho en Fase 0)

Esto protege contra: abrir el `.exe` en un editor de texto o correrle `strings` y esperar ver
el código AvaLang o la clave en texto plano — ya no aparecen.

Esto **no** protege contra: un atacante con debugger o que vuelca la memoria del proceso en
ejecución. La clave reconstruida y el código descifrado tienen que existir en claro en algún
punto de la memoria del proceso para que el programa funcione — eso es inherente a cualquier
esquema donde el descifrador viaja junto con el secreto, no un bug de esta implementación.

### Pendiente de tu lado

Recompilar y correr `strings` (o equivalente en Windows, ej. `strings.exe` de Sysinternals, o
abrir el `.exe` en un editor hex) sobre un `.exe` empacado para confirmar que no aparece
código AvaLang ni la clave en claro — es la prueba de "ataque casual" que pide el plan.

---

## Fase 4 — minimizar la ventana de exposición en disco

**Cambio de arquitectura respecto a Fase 3, no bloqueante pero avisado:** para lograr
"descifrar cada archivo justo antes de que el compilador lo pida" (como pide el plan), hizo
falta un cambio quirúrgico en `runtime/avalang` — no solo en `avapack`. La razón: `DoImport`
(`runtime/avalang/src/vm/vm_import.cpp`) abre cada módulo importado con un `std::ifstream`
crudo, en el momento en que ese `import` se ejecuta (no antes) — no hay ningún punto de
extensión previo desde fuera de `avalang.dll` para interceptar esa lectura. Agregué dos hooks
opcionales a `ava::VM` (`SetBeforeModuleReadHook` / `SetAfterModuleReadHook`, en
`runtime/avalang/src/vm/vm.h` + `vm_core.cpp`), mismo patrón ya usado por `PrintSink` /
`AlertSink` / `NavigateSink` en esa misma clase: sin hook instalado (el caso de `ava_cli` y
`avahost` normales, que nunca los tocan) el comportamiento es idéntico al de antes de este
cambio, byte por byte. Es la misma clase de ajuste que ya había hecho falta en Fase 2 con
`AVA_PLATFORM_API` — avisado igual que aquella vez por si preferís que lo revierta y busque
otra vía antes de Fase 5.

### Qué cambia respecto a Fase 3

- **Entry file:** ya no se escribe a disco en ningún momento. Se descifra directo a memoria y
  se pasa como `std::string` a `ava_compile` (que ya aceptaba el source como string — esto ya
  estaba disponible, Fase 3 simplemente no lo aprovechaba porque volcaba todo a un temp dir
  primero).
- **Módulos importados** (`import X` dentro de cualquier `.ava`/`.avaui` del proyecto): el
  temp dir arranca **vacío**. `SetBeforeModuleReadHook` descifra el archivo puntual que
  `DoImport` está por abrir y lo escribe en la ruta resuelta justo antes de que el
  `std::ifstream` la abra; `SetAfterModuleReadHook` sobreescribe esos bytes con ceros y borra
  el archivo apenas termina el `file.close()` — no hace falta esperar a que termine de
  compilar ese módulo, porque el código fuente ya está copiado a un `std::string` en memoria
  en ese punto (ver `ModuleCache`, que solo cachea el `Proto` ya compilado, nunca vuelve a leer
  el archivo físico — confirmado en `runtime/avalang/src/vm/module.cpp`).
- En Windows, cada archivo materializado se marca `FILE_ATTRIBUTE_TEMPORARY` (mitigación de
  cache del SO, no una garantía — ver comentario en `main.cpp`).
- El temp dir en sí ya vive bajo `%TEMP%` del usuario actual por default en Windows (no hace
  falta código adicional para eso — el punto del plan sobre "carpeta temp restringida al
  usuario" ya se cumplía desde Fase 3, documentado ahora explícitamente).
- `TempDirGuard` (borra `temp_dir` completo al salir) se mantiene como red de seguridad para
  caminos de error/crash — en el camino feliz ya debería encontrar el directorio vacío.

### Qué NO cambia (contrato estable)

`embedded_project.h`, `avapack_gen` y el formato cifrado no se tocaron en esta fase — el
`embedded_project.cpp` que ya generaste para Fase 3 sigue siendo válido, no hace falta
regenerarlo.

### Verificado de mi lado

Sin poder compilar `avalang.dll` completo acá (requiere el toolchain de Windows/vcpkg, ver
limitación ya conocida de fases anteriores), sí pude: (1) chequeo de sintaxis (`g++
-fsyntax-only`) de los tres archivos tocados de `avalang` (`vm.h`, `vm_core.cpp`,
`vm_import.cpp`) y del nuevo `main.cpp` de `avapack`; (2) un harness aparte que reproduce la
lógica exacta de los hooks (descifrar → escribir → comparar byte a byte contra el original →
sobreescribir con ceros → borrar) contra los 19 archivos de `samples/web/testproj`, confirmando
0 diferencias y 0 archivos remanentes bajo el temp dir al terminar. Lo que sigue sin poder
probarse acá es el flujo real dentro de la VM (que un `import` disparado en runtime realmente
disponga del archivo en el instante justo) — eso queda para tu compilación completa.

### Pendiente de tu lado (requiere compilar)

- Confirmar que `runtime/avapack_testproj.exe` (o el nombre que le hayas dado) sigue corriendo
  igual que en Fase 3 contra `samples/web/testproj` (import de `services/catalog.ava` incluido).
- Con `Process Monitor` (Sysinternals) o similar: confirmar que nunca hay más de un `.ava`/
  `.avaui` en claro a la vez bajo el temp dir mientras el proceso corre.
- Confirmar que `ava_cli`/`avahost` normales (sin pasar por `avapack`) siguen compilando y
  corriendo exactamente igual que antes — los hooks nuevos en `vm.h` no deberían tocarlos, pero
  vale la pena la prueba de regresión ya que es un archivo compartido por todo el runtime.

---

## Fase 5 — hardening adicional

Tres mejoras incrementales, cada una independiente de las otras (se puede adoptar una sin las
demás):

### 1) Anti-tampering: verificación de integridad (HMAC-SHA256)

**Qué se agregó:** `runtime/avapack/src/checksum/sha256.h`/`.cpp` — SHA-256 y HMAC-SHA256
implementados desde cero para este proyecto (no vendorizados; no dependen de OpenSSL ni de
ninguna otra librería). Verificados contra los vectores de prueba oficiales (NIST para SHA-256,
RFC 4231 para HMAC-SHA256) — ver el propio archivo de test que usé para validarlos, no se
commiteó porque era solo para esta verificación puntual.

`avapack_gen` calcula un HMAC-SHA256 sobre la concatenación de `path + cipher + nonce` de cada
archivo embebido (mismo orden determinista que ya usa el resto del generador), firmado con la
propia clave AES-256 real del build — **no se agrega ningún secreto nuevo al binario**, se
reutiliza el que ya existía para el cifrado. El resultado se embebe como `kIntegrityMac[32]`
(ver `embedded_project.h`). `main.cpp` lo recalcula al arrancar, apenas reconstruye la clave y
antes de descifrar o compilar nada — si no coincide, el programa termina con exit code 1 sin
llegar a exponer ningún byte de código en claro.

**Qué detecta:** alguien modificó bytes del array `kEmbeddedFiles`/`kIntegrityMac` dentro del
`.exe` ya compilado (por ejemplo, para reemplazar un archivo cifrado por otro, o para intentar
"apagar" el chequeo a mano sin recompilar — cualquier bit que cambie en esos arrays invalida el
HMAC).

**Qué NO previene** (mismo espíritu que el resto de este documento): un atacante dispuesto a
parchear el binario también puede parchear el propio chequeo — saltear el `if` en el
disassembly, o recalcular el HMAC con la clave que de todas formas tiene que poder reconstruir
para que el programa corra. Es detección de manipulación casual, no una garantía criptográfica
de integridad contra un atacante con herramientas de reversing.

### 2) Firma de código del `.exe` final

**Qué se agregó:** `ava_cli build` ahora acepta `--sign-pfx <ruta.pfx>` (+ `--sign-password-env
<VAR>` y `--sign-timestamp-url <url>`, ambos opcionales) — ver
`runtime/avacli/src/build_command.cpp`. Si se pasa, después de copiar el `.exe` final a `--out`
se invoca `signtool sign /f <pfx> /fd sha256 [/p <password>] [/tr <url> /td sha256] <exe>` vía
`IProcess` (mismo mecanismo que ya se usa para invocar `cmake`). La password del `.pfx`, si
tiene, se lee de una variable de entorno — nunca se pasa como argumento de línea de comandos
directo ni queda en ningún log de este comando.

También hay un script standalone equivalente, `scripts/build/sign_release.bat`, para firmar un
`.exe` ya construido sin volver a pasar por `ava_cli build` (útil para re-firmar un build viejo,
o para depurar un problema de firma).

**Importante — esto es sobre el `.exe` en sí, no sobre el cifrado del código AvaLang:** firmar
el binario le dice a Windows/SmartScreen (y a quien lo reciba) "esto lo publicó tal identidad y
no fue modificado desde que se firmó" — no tiene relación con el cifrado AES de Fase 3 ni con el
HMAC del punto anterior, son tres mecanismos independientes con propósitos distintos.

**No incluido, y fuera de alcance de este cambio:** conseguir o gestionar el certificado de
firma de código en sí (comprarlo a una CA pública, o generarlo con una CA interna). Este es un
paso de infraestructura/proceso de tu organización, no algo que el código de `avapack`/`ava_cli`
pueda resolver — `--sign-pfx` asume que ya tenés el `.pfx` a mano.

Solo soportado en Windows (`signtool` es una herramienta del Windows SDK); en otras plataformas
`ava_cli build --sign-pfx ...` falla temprano con un mensaje claro en vez de intentar invocar un
comando que no existe ahí.

### 3) Modo debug empacado

**Qué se agregó:** flag `--debug` en `avapack_gen` y en `ava_cli build` (que la propaga vía
`-DAVAPACK_DEBUG_UNENCRYPTED=ON` a CMake, ver `runtime/avapack/CMakeLists.txt`). Con `--debug`:

- El contenido embebido queda **en claro** (no pasa por AES-256-CTR) — `kDebugBuild = true` en
  el `embedded_project.cpp` generado le indica a `main.cpp` que no debe descifrar, solo copiar
  los bytes tal cual (aplicar CTR sobre texto que ya está en claro lo corrompería, no lo
  "descifriría" — CTR es su propia inversa solo si el otro lado realmente cifró con la misma
  clave/nonce).
- `ava_cli build --debug` compila en modo `Debug` (símbolos, sin optimizar) en vez de `Release`.
- El HMAC de integridad (punto 1) se sigue calculando y verificando igual, sobre el contenido en
  claro — detectar tampering no depende de que el contenido esté cifrado.

**Para qué sirve:** diagnosticar un bug de un proyecto ya empacado (con un debugger real, viendo
símbolos y el código fuente tal cual) sin tener que romper a mano el cifrado de un build de
producción, ni recompilar el proyecto original fuera de `avapack`.

**Uso:** `ava_cli build --project <dir> --entry <archivo.ava> --out debug.exe --debug`. El
binario resultante **no debe distribuirse** — pierde toda la protección de Fase 3/4. Esto queda
documentado también en el `--help` de `ava_cli build` y en un mensaje que el propio comando
imprime cuando se usa `--debug`.

### Pendiente de tu lado (requiere compilar)

- Recompilar y correr el generador (`avapack_gen`) contra un proyecto real, en modo normal y con
  `--debug`, y confirmar que el `.exe` empacado sigue arrancando bien en ambos casos.
- Parchear a mano un byte cualquiera de `kEmbeddedFiles` en un `embedded_project.cpp` ya
  generado (o del binario compilado) y confirmar que el `.exe` se niega a correr con el mensaje
  de "verificacion de integridad fallida" — es la prueba real de que el HMAC efectivamente
  detecta el tampering.
- Si tenés un certificado de firma de código de prueba: correr `ava_cli build --sign-pfx ...` (o
  `scripts/build/sign_release.bat` sobre un `.exe` ya construido) y confirmar con
  `signtool verify /pa` que la firma quedó válida.
- Confirmar que `ava_cli build` **sin** ninguna de las flags nuevas de Fase 5 (`--debug`,
  `--sign-pfx`) sigue comportándose exactamente igual que en Fase 4 — ninguna de las dos debería
  cambiar nada si no se piden explícitamente.

## Fase 6 — bytecode en vez de fuente (`.avbc`)

Ver `plan_ava_pack.md`, Fase 6, para el detalle completo (formato de serialización, las tres
partes de ofuscación, la API pública nueva en `avalang.h`). Resumen: `ava_cli build
--obfuscate [--obfuscate-strings] [--flatten-control-flow]` compila y serializa el `--entry` a
un módulo `.avbc` precompilado (en vez de embeber su `.ava` en texto plano) antes de que
`avapack_gen` lo cifre igual que a cualquier otro archivo — el runtime empacado (`main.cpp` o,
desde Fase 7, `main_zerodisk.cpp`) lo detecta vía `avapack::kEntryIsBytecode` y lo deserializa
directo con `ava_module_deserialize` en vez de pasar por `ava_compile`/el frontend ANTLR. Los
imports (`import "..."` dentro del proyecto) siguen siendo `.ava`/`.avaui` en texto plano
cifrado — precompilar el árbol completo de imports queda fuera de esta fase.

## Fase 7 — filesystem virtual en memoria (cero disco)

**Qué se agregó:** una plantilla de runtime empacado alternativa,
`runtime/avapack/src/main_zerodisk.cpp`, seleccionable con `ava_cli build --zero-disk`
(`-DAVAPACK_ZERO_DISK=ON` en CMake). En vez del esquema de Fase 4 (temp dir + hooks
antes/después de que `DoImport` abre cada archivo, uno a la vez), instala un
`MemoryFileSystem` (`runtime/avalang/platform/memory/MemoryFileSystem.h/.cpp`) como el
`IFileSystem` activo — así que **ningún** `.ava`/`.avbc` del proyecto llega a existir como
archivo real en disco, ni por milisegundos.

### Cómo se conecta al VM sin tocar el ABI del PAL

`platform/interfaces/IFileSystem.h` sigue `STABLE` (`PAL_ABI.h`) sin cambios — `MemoryFileSystem`
es solo una implementación nueva de esa interfaz. El punto de inyección nuevo es
`VmPlatformAccessor::SetOverride(std::unique_ptr<IPlatform>)` /
`ClearOverride()` (`runtime/avalang/src/vm/vm_platform_accessor.h/.cpp`), protegido con un
`std::mutex` — `Get()` primero garantiza (Meyer's Singleton, como antes) que el `IPlatform`
real esté construido, y después devuelve el override si hay uno instalado. `ava_cli`/`avahost`
normales nunca llaman `SetOverride`, así que quedan bit a bit iguales a antes de esta fase.

`main_zerodisk.cpp`:

1. Verifica integridad y reconstruye la clave AES-256 — igual que `main.cpp`.
2. Crea el `IPlatform` real (`platform::Platform::Create()`) y un `MemoryFileSystem` que cae a
   ESE real como *fallback* — necesario para que el stdlib real (que sigue viviendo en disco al
   lado de `avalang.dll`) se siga resolviendo con normalidad. Solo el proyecto empacado es
   "zero disk", no cualquier I/O que el programa haga.
3. Registra cada `EmbeddedFile` (salvo el entry, que se maneja aparte, igual que en `main.cpp`)
   bajo un prefijo sentinela (`avapack:/vfs`, **no** una ruta real) con un `ContentProvider` que
   descifra **bajo demanda** en cada `ReadFile()` — nunca precalcula ni cachea el plaintext de
   todos los archivos de una — y captura la clave por referencia, mismo patrón que ya usaban los
   hooks de Fase 4.
4. Instala el override (`MemoryOverridePlatform`, que delega todo excepto `FileSystem()` al
   `IPlatform` real) vía `SetOverride` — **antes** de crear el `AvaVM`.
5. Descifra+compila/deserializa el entry en memoria exactamente igual que `main.cpp`
   (`kEntryIsBytecode`/`kEntryStringsObfuscated`, Fase 6) y corre. Sin `TempDirGuard`, sin
   hooks — no aplican, este flujo no crea nada en disco para empezar.

### Bug real encontrado (no cosmético): `DoImport` bypaseaba el PAL

Al revisar el punto de extensión, `ModuleResolver::ResolveModulePath` (`module.cpp`) ya pasaba
siempre por `VmPlatformAccessor::Get().FileSystem()` — pero **`VM::DoImport`
(`vm_import.cpp`) leía el contenido del módulo con `std::ifstream` crudo**, sin pasar por el
PAL en absoluto. Un `MemoryFileSystem` instalado como override habría quedado inerte: el
`Exists()` de `ResolveModulePath` sí lo hubiera "encontrado" (porque ese sí pasa por el PAL),
pero al intentar leerlo, `DoImport` hubiera intentado abrir literalmente `avapack:/vfs/...` con
`std::ifstream` — que no es una ruta real — y fallado con "could not open module file".

**Corregido:** `DoImport` ahora lee vía `VmPlatformAccessor::Get().FileSystem().ReadFile(...)`,
igual que `ModuleResolver`. Sin ningún override instalado (`ava_cli`, `avahost`, o el `main.cpp`
de Fase 4-6 sin `--zero-disk`), esto pasa por `WinFileSystem::ReadFile` — funcionalmente
idéntico a lo que hacía el `std::ifstream` de antes — así que no hay cambio de comportamiento
fuera de Fase 7. Este fix vive en `runtime/avalang` (no en `avapack`), mismo tipo de desvío ya
documentado en Fase 4 con los hooks `SetBeforeModuleReadHook`/`SetAfterModuleReadHook`.

### Alcance real (no lo que decía el plan original)

El filesystem virtual cubre los imports del proyecto — lo que pasa por
`ModuleResolver`/`DoImport`. Cualquier I/O explícito que el propio código `.ava` del usuario
haga contra archivos externos (si el stdlib expone funciones de lectura de archivos) sigue
yendo a disco real, vía el *fallback* de `MemoryFileSystem` al `IFileSystem` real — eso es
intencional, Fase 7 protege el código del proyecto empacado, no reemplaza el I/O que el
programa necesite hacer de por sí.

### Compatible con Fase 5/6

`--zero-disk` es ortogonal a `--debug`, `--obfuscate`/`--obfuscate-strings`/
`--flatten-control-flow` y `--sign-pfx` — todos actúan sobre el contenido embebido o sobre el
`.exe` final, no sobre el mecanismo de materialización de imports. No hay interacción de código
entre ellos más allá de que `AVAPACK_ZERO_DISK` elige qué archivo `.cpp` se compila como
`main()`.

### Pendiente de tu lado (requiere compilar y, para la prueba de humo, Windows)

- Compilar con `ava_cli build --project <dir> --entry <archivo.ava> --out packed.exe
  --zero-disk` y confirmar con Process Monitor (Sysinternals) que el `.exe` resultante no crea
  ni un solo archivo bajo `%TEMP%` (ni en ningún otro lado) durante toda su ejecución.
- Confirmar `--zero-disk --obfuscate` y `--zero-disk --debug` de punta a punta contra el
  compilador ANTLR real (no lo pude ejercitar sin el toolchain de Windows/vcpkg).
- Confirmar que un proyecto multi-archivo con imports anidados (`import a.b.c`, `index.ava` de
  carpeta) resuelve igual bajo `--zero-disk` que bajo el flujo por defecto — la lógica de
  `ModuleResolver` no cambió, pero no se corrió contra el `MemoryFileSystem` real.

## Fase 9 — build sin depender del repo (stub precompilado + payload apendeado)

**El problema:** hasta Fase 8, `ava_cli build --target desktop` recompilaba avapack entero
(`main.cpp` + un `embedded_project.cpp` generado con el proyecto embebido como constantes
C++) vía CMake, en `build_pack/`, en **todos los casos**. Aunque `avalang.dll`/`avalang_ui.dll`
estuvieran prebuilt junto a `ava_cli.exe` (`AVA_PACK_USE_PREBUILT_AVALANG`), seguía haciendo
falta un checkout completo de AvaLang al lado (`runtime/avapack/CMakeLists.txt`,
`third_party/tiny-aes-c`, `checksum/`, un compilador C++) solo para volver a traducir el
mismo código de `main.cpp` una y otra vez, cambiando únicamente el array de bytes cifrados
embebido. Eso hacía imposible distribuir `ava_cli` como herramienta standalone para empacar
proyectos de terceros.

**La solución:** separar "el código que corre el proyecto empacado" (que no cambia de un
empacado a otro) de "los datos del proyecto empacado" (que sí cambian), y unirlos en runtime
en vez de en tiempo de compilación:

1. `avapack_stub.exe` — un binario genérico, prebuilt una sola vez (`scripts/build_pack_tools.bat`),
   **sin ningún proyecto embebido**. Es `stub_main.cpp` compilado: al arrancar, ubica su propio
   ejecutable en disco, lee los últimos `kFooterSize` bytes (`PayloadFooter`,
   `src/payload_format.h`) para encontrar el offset/tamaño de un blob apendeado más atrás en el
   mismo archivo, decodifica ese blob (`DecodePayloadBlob`) a un `avapack::PayloadBlob`, arma la
   vista `EmbeddedFile[]` equivalente a la que antes generaba `embedded_project.cpp`
   (`BuildEmbeddedFilesView`) y llama a `RunPackagedProgram` (ver más abajo) — mismo camino de
   ejecución que `main.cpp` de Fases 1-8, solo que los datos vienen de un blob leído en runtime en
   vez de constantes compiladas.
2. `avapack_gen --payload-out <ruta.bin>` (`src/generator/main.cpp`) — el mismo generador de
   siempre (compila/lee el proyecto, cifra con AES-256-CTR, calcula el HMAC-SHA256, ver Fase 3/5),
   pero en vez de emitir un `.cpp` para compilar, serializa un `PayloadBlob` con
   `EncodePayloadBlob` a un archivo binario suelto. `--out <ruta.cpp>` (el camino viejo, para el
   flujo con CMake) y `--payload-out` son independientes — se puede pedir uno, el otro, o ambos.
3. `ava_cli build --target desktop` (`TryFastPackWithPrebuiltStub`, `runtime/avacli/src/build_command.cpp`):
   si encuentra `avapack_stub.exe` + `avapack_gen.exe` prebuilt junto a `ava_cli.exe` (mismo
   directorio, vía `FindPrebuiltPackTools`) **y** las DLL de avalang (`avalang.dll`/
   `avalang_ui.dll`, que el stub necesita para arrancar), corre `avapack_gen --payload-out` a un
   archivo temporal, copia `avapack_stub.exe` a `--out`, le apendea el blob y por último el
   `PayloadFooter` (offset/tamaño fijo, `EncodeFooter`) — sin invocar CMake ni necesitar
   `runtime/avapack/CMakeLists.txt` (ni ningún otro archivo del repo) para nada más que
   `--extra-modules-dir` opcional (`libraries/`, si existe al lado). Firma con `--sign-pfx` y
   copia de DLL/librerías nativas externas funcionan igual que en el flujo viejo.

### Refactor que lo hizo posible: `packaged_runtime.h/.cpp`

Antes, toda la lógica de "ya tengo el proyecto descifrado en memoria, ahora corré esto" vivía
inline en `main()` de `main.cpp` (temp dir, hooks de Fase 4, verificación de integridad,
descifrado de cada archivo, bytecode vs. fuente de Fase 6). Se extrajo tal cual a
`RunPackagedProgram(const PackagedManifest&)` en `packaged_runtime.h/.cpp`, parametrizada por
un `PackagedManifest` (equivalente a lo que antes eran las constantes globales que generaba
`embedded_project.cpp`). `main.cpp` (Fases 1-8, sigue existiendo para el flujo con CMake) quedó
como un wrapper delgado que arma el `PackagedManifest` desde las constantes compiladas de
siempre y llama a `RunPackagedProgram` — mismo comportamiento bit a bit que antes de esta fase.
`stub_main.cpp` arma el mismo `PackagedManifest`, pero desde el blob leído en runtime. Ningún
cambio en `embedded_crypto.h`/`embedded_project.h` originales — se agregaron variantes
parametrizadas (`DecryptWith`/`VerifyIntegrityWith`/`BuildFileMapFrom`/`GetKeyFromFragments`)
sin tocar las funciones que ya usaba `main.cpp`, para no arriesgar Fases 1-8.

### Formato del blob apendeado (`src/payload_format.h`)

Apendeado al final de una copia de `avapack_stub.exe`: `[.exe stub][blob][PayloadFooter (32
bytes fijos)]`. El footer siempre son los últimos 32 bytes del archivo (`magic` de 8 bytes
`"AVAPKFT1"`, `version`, `flags` reservado, `blob_offset`, `blob_size` — todo little-endian, sin
padding implícito de struct). `stub_main.cpp` lee esos últimos 32 bytes del propio ejecutable
para encontrar el blob sin tener que parsear el formato PE/ELF del `.exe` en sí. El blob en sí
serializa lo mismo que antes codificaba `embedded_project.cpp` como constantes C++: semilla +
fragmentos de clave, MAC de integridad, flags de bytecode/ofuscación, y la lista de
`EmbeddedFile` (ruta, nonce, contenido cifrado) — ver `PayloadBlob`/`EncodePayloadBlob`/
`DecodePayloadBlob` para el detalle campo por campo.

### Alcance real / qué NO cubre todavía

- **`--zero-disk` (Fase 7) no está wireado al stub.** `TryFastPackWithPrebuiltStub` lo detecta
  (`opts.zero_disk`) y devuelve `false` sin intentar nada, así que `ava_cli build --zero-disk`
  cae automáticamente al flujo viejo con CMake (que sí lo soporta, vía `main_zerodisk.cpp`) — no
  rompe nada, simplemente no es "rápido" para ese caso todavía. Pendiente: un
  `stub_main_zerodisk.cpp` análogo, o unificar ambos stubs con un flag en el footer.
- **`--target barekernel` tampoco pasa por este camino** — sigue siendo exclusivamente
  CMake + toolchain cruzado i686-elf (`RunBuildBarekernelCommand`), sin relación con
  `avapack_stub.exe`. No tendría sentido unificarlo: barekernel no corre sobre un SO que pueda
  ejecutar un `.exe` genérico con un blob apendeado.
- El flujo con CMake (`build_pack/`, target `avapack_build`) sigue existiendo tal cual y es el
  fallback automático cuando `avapack_stub.exe`/`avapack_gen.exe` no están prebuilt junto a
  `ava_cli.exe` — `ava_cli build` ahora solo exige un checkout completo de AvaLang
  (`runtime/avapack/CMakeLists.txt` presente en `--repo-root`) en ese caso, no siempre.

### Pendiente de tu lado (requiere compilar — no lo pude ejercitar sin el toolchain de Windows)

- Compilar con `scripts/build_pack_tools.bat` y confirmar que genera `avapack_gen.exe` +
  `avapack_stub.exe` junto a `avalang.dll`/`avalang_ui.dll`.
- Correr `ava_cli build --project <dir> --entry <archivo.ava> --out packed.exe` con esos
  binarios prebuilt junto a `ava_cli.exe` **sin** `runtime/avapack/CMakeLists.txt` al lado (por
  ejemplo, copiando solo `ava_cli.exe` + `avapack_stub.exe` + `avapack_gen.exe` + las DLL a una
  carpeta vacía) y confirmar que arma y corre `packed.exe` igual que el flujo con CMake.
- Confirmar que, si `avapack_stub.exe`/`avapack_gen.exe` NO están prebuilt, `ava_cli build` sigue
  funcionando exactamente igual que antes de esta fase (fallback a CMake) cuando SÍ hay un
  checkout completo al lado, y falla con el mensaje de error nuevo (no el genérico de CMake)
  cuando no hay ninguna de las dos cosas.
- Confirmar `--sign-pfx`, `--obfuscate` (incluyendo que el `.avmap` se copia y el temporal se
  borra) y `--extra-modules-dir`/`libraries/` de punta a punta por este camino nuevo.
- `--zero-disk` por este camino queda explícitamente fuera de alcance por ahora (ver arriba) —
  no hace falta probarlo, solo confirmar que cae al flujo viejo sin error.

## Estado

- [x] Fase 0 — decisiones de diseño (este documento).
- [~] Fase 1 — empacador básico funcional (sin cifrar). Código listo
      (`avapack_gen`, plantilla `main.cpp`, `CMakeLists.txt`); falta que
      compiles y confirmes las 3 pruebas manuales (ver `plan_ava_pack.md`).
- [~] Fase 2 — integración como subcomando `ava_cli build`. Código listo
      (`build_command.cpp/.h`, `Platform::Create()` ahora exportado); falta
      la prueba end-to-end de tu lado.
- [~] Fase 3 — cifrado del contenido embebido (AES-256-CTR, ver sección de
      arriba). Código listo; falta la prueba de `strings` de tu lado, y que
      confirmes el desvío GCM→CTR.
- [~] Fase 4 — minimizar ventana de exposición en disco (ver sección de
      arriba). Código listo, incluye un cambio quirúrgico en
      `runtime/avalang` (hooks opcionales en `ava::VM`); falta la prueba
      end-to-end contra `avalang.dll` real y la de `Process Monitor` de tu
      lado.
- [~] Fase 5 — hardening adicional (ver sección de abajo). Código listo
      para los 3 puntos del plan; falta compilar y probar de tu lado.
- [ ] Fase 6 — bytecode en vez de fuente (roadmap largo plazo).
- [ ] Fase 7 — filesystem virtual en memoria (roadmap largo plazo).
- [ ] Fase 8 — multiplataforma (bloqueada por trabajo externo).
- [~] Fase 9 — build sin depender del repo (stub precompilado + payload
      apendeado, ver sección de arriba). Código listo (`payload_format.h`,
      `packaged_runtime.h/.cpp`, `stub_main.cpp`, `avapack_gen --payload-out`,
      `TryFastPackWithPrebuiltStub` ya conectado en `RunBuildCommand`); falta
      compilar `scripts/build_pack_tools.bat` y correr las pruebas manuales
      de tu lado. `--zero-disk` explícitamente no soportado todavía por este
      camino (cae a CMake, ver "Alcance real" arriba).
