/* app.ld - Linker script for 32-bit kernel applications
 *
 * Portado VERBATIM del template real de AppBuilder
 * (Core/LinkerScriptManager.cs, LINKER_SCRIPT_TEMPLATE, confirmado contra
 * AppBuilder.zip) -- Fase B2 de plan_avapack_barekernel.md. AppBuilder
 * mismo lo autogenera con este contenido exacto si el .ld no existe en el
 * proyecto (GetLinkerScriptPath()), asi que copiarlo tal cual acá es lo
 * que garantiza el mismo layout de secciones que produce el AppBuilder
 * real.
 *
 * Requiere que el compilador use -ffunction-sections (ver
 * runtime/avapack/src/barekernel/CMakeLists.txt / flags de
 * avapack_barekernel_app) para que _start caiga en su propia sección
 * .text._start -- si no, KEEP(*(.text._start)) de abajo no encuentra nada
 * que preservar y el link falla con "undefined reference to _start" o
 * termina en un offset inesperado.
 */

ENTRY(_start)

SECTIONS
{
    . = 0x40000000;  /* Address where kernel will load this app */

    /* Code section: _start must explicitly go first */
    .text ALIGN(4) : {
        KEEP(*(.text._start))     /* Ensure _start is not eliminated */
        *(.text*)
        *(.gnu.linkonce.t*)
    }

    /* Read-only data section */
    .rodata ALIGN(4) : {
        *(.rodata*)
        *(.gnu.linkonce.r*)
    }

    /* Initialized data section */
    .data ALIGN(4) : {
        *(.data*)
        *(.gnu.linkonce.d*)
    }

    /* BSS section (uninitialized data) */
    .bss ALIGN(4) : {
        __bss_start = .;
        *(.bss*)
        *(.gnu.linkonce.b*)
        *(COMMON)
        __bss_end = .;
    }

    /* Discard unnecessary sections */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
