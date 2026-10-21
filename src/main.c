#include <vitasdkkern.h>
#include <psp2kern/kernel/threadmgr.h>
#include <taihen.h>

/*
 * VirtualVitaRAStick - bootstrap kernel plugin
 *
 * Phase 0 proves that the VitaSDK toolchain can produce a loadable .skprx.
 * Keep one harmless real kernel import in the bootstrap so the linker emits
 * the Vita import/relocation metadata required by vita-elf-create.
 */

int module_start(SceSize argc, const void *args) {
    (void)argc;
    (void)args;

    /* 1 us delay: harmless at startup, but forces a real kernel import. */
    ksceKernelDelayThread(1);

    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
    (void)argc;
    (void)args;
    return SCE_KERNEL_STOP_SUCCESS;
}

void _start() __attribute__((weak, alias("module_start")));
