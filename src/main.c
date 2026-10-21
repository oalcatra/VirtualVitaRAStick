#include <vitasdkkern.h>
#include <taihen.h>

/*
 * VirtualVitaRAStick - bootstrap kernel plugin
 *
 * Phase 0 deliberately does not hook input yet. The first milestone is to
 * prove that our source, VitaSDK toolchain and GitHub Actions pipeline can
 * produce a valid .skprx before we touch SceTouch/SceCtrl.
 */

int module_start(SceSize argc, const void *args) {
    (void)argc;
    (void)args;
    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
    (void)argc;
    (void)args;
    return SCE_KERNEL_STOP_SUCCESS;
}
