# tiny-AES-c (vendorizado)

Fuente: https://github.com/kokke/tiny-AES-c (rama `master`)
Licencia: Unlicense / dominio público (ver `LICENSE` en esta carpeta).
Archivos: `aes.h`, `aes.c`, sin modificaciones respecto al upstream.

## Nota importante (Fase 3 de `plan_ava_pack.md`)

Fase 0 propuso "AES-256 con tiny-AES-c" sin bajar al detalle del modo de cifrado; el plan de
Fase 3 después pidió específicamente **AES-GCM**. Al vendorizar la librería (esta rama del
repo, la misma que ya estaba referenciada desde Fase 0) confirmo que **no incluye GCM** — solo
ECB, CBC y CTR. No hay una versión "oficial" de tiny-AES-c con GCM.

Decisión: uso **AES-256-CTR** en lugar de GCM, manteniendo la misma librería ya elegida en
Fase 0 (no vendorizo una segunda librería de cifrado solo para tener AEAD). Motivo por el que
esto no resigna nada respecto al modelo de amenaza ya documentado en el README:

- El modelo de amenaza de este empacador (ver `runtime/avapack/README.md`, sección Fase 3) es
  explícitamente "disuasivo contra extracción casual (`strings`)", no "resistente contra un
  atacante con debugger". GCM aporta **autenticación** (detectar si el ciphertext fue
  modificado), no **confidencialidad** adicional sobre CTR — y la detección de tampering del
  binario ya está prevista aparte, más adelante, en Fase 5 ("checksum del propio binario
  embebido y verificado al arrancar").
- Con CTR, cada archivo usa un nonce/IV de 16 bytes aleatorio y distinto (nunca reusado con la
  misma clave dentro del mismo build), que es el requisito de seguridad real de este modo.

Si en algún momento se necesita autenticación real del contenido embebido, la vía es agregar
un HMAC (o similar) sobre el ciphertext en Fase 5, no volver a esta decisión de Fase 3.
