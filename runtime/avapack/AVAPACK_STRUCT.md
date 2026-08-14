# AVAPACK_STRUCT.md — estructura de `runtime/avapack/`

Documento corto de referencia: qué vive dónde dentro de `avapack`, para que quede claro desde
el día uno (antes de que haya código real en la carpeta).

```text
runtime/avapack/
├── README.md              Decisiones de diseño (Fase 0) + estado de fases.
├── AVAPACK_STRUCT.md       Este archivo.
├── CMakeLists.txt          (Fase 1) Target(s) avapack_gen / plantilla de empacado.
├── src/
│   ├── main.cpp            (Fase 1, cifrado agregado en Fase 3) Plantilla de runtime
│   │                        empacado por defecto: reconstruye la clave, descifra cada
│   │                        archivo embebido en memoria, lo vuelca a temp (uno a la vez,
│   │                        Fase 4), arranca el VM, compila/deserializa y corre el entry.
│   ├── main_zerodisk.cpp   (Fase 7) Plantilla alternativa, seleccionada con
│   │                        -DAVAPACK_ZERO_DISK=ON (`ava_cli build --zero-disk`): en vez de
│   │                        temp dir + hooks, instala un MemoryFileSystem (ver
│   │                        runtime/avalang/platform/memory/) como override del IPlatform
│   │                        activo -- ningún import toca disco real, ni por milisegundos.
│   │                        Mutuamente excluyente con main.cpp (CMakeLists.txt elige uno
│   │                        de los dos, nunca ambos).
│   ├── embedded_crypto.h  (Fase 7) Decrypt/VerifyIntegrity/FileMap/BuildFileMap,
│   │                        extraídos de main.cpp para no duplicarlos entre main.cpp y
│   │                        main_zerodisk.cpp -- ambos necesitan exactamente la misma
│   │                        lógica de descifrado/integridad.
│   ├── generator/           (Fase 1, cifrado agregado en Fase 3) Fuente de avapack_gen:
│   │                        recorre --project, cifra cada archivo (AES-256-CTR) y produce
│   │                        embedded_project.cpp acorde al contrato de embedded_project.h.
│   ├── embedded_project.h   (Fase 1, actualizado en Fase 3 y Fase 5) Contrato struct
│   │                        EmbeddedFile{path, cipher, content_len, nonce} + la función de
│   │                        reconstrucción de clave (GetEmbeddedKey), + (Fase 5)
│   │                        kIntegrityMac/kDebugBuild. Vive en src/ como header compartido
│   │                        entre el generador y la plantilla, no generado dinámicamente.
│   └── checksum/             (Fase 5) sha256.h/.cpp: SHA-256 + HMAC-SHA256 propios (no
│                              vendorizados -- implementación de avapack, no de terceros).
│                              Usado por avapack_gen (calcular kIntegrityMac) y por
│                              main.cpp/main_zerodisk.cpp (recalcularlo y verificarlo).
└── third_party/
    └── tiny-aes-c/          (Fase 3) aes.h/aes.c vendorizados sin modificar (ver VENDOR.md
                              para la nota de por qué CTR y no GCM). Se compila con
                              AES256=1 CTR=1 CBC=0 ECB=0 tanto en avapack_gen como en el
                              .exe empacado final.
```

## Convenciones

- Todo lo que `avapack_gen` genera en tiempo de build (`embedded_project.cpp` con el
  contenido de cada archivo del proyecto) es un artefacto de build — vive en la carpeta de
  build intermedia (`build_pack/...`), **nunca** se commitea ni vive dentro de `src/`.
- `embedded_project.h` sí es código fuente versionado: define el contrato (`struct
  EmbeddedFile`) que tanto el generador como la plantilla de `main.cpp` conocen en tiempo de
  compilación.
- Ningún archivo bajo `runtime/avapack/` debe ser importado por `runtime/avalang/` (el core
  del VM no depende de avapack, es al revés).
