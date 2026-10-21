#include <vitasdkkern.h>
#include <psp2kern/ctrl.h>
#include <psp2/touch.h>
#include <taihen.h>

int ksceTouchPeek(SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs);

#define CTRL_HOOKS 6
#define REQUIRED_HOOKS 4
#define TOUCH_CENTER 128
#define TOUCH_RADIUS 180
#define RIGHT_ZONE_MIN_X 960

#define NID_PEEK_POS 0xEA1D3A34
#define NID_READ_POS 0x9B96A1AA
#define NID_PEEK_NEG 0x19895843
#define NID_READ_NEG 0x8D4E0DD1

/* Diagnostic build: test only the Positive2 family on top of the known-good
 * positive/negative export hooks. These offsets target SceCtrl on firmware
 * 3.60 and are optional so failure to install them cannot disable the plugin. */
#define OFF_PEEK_POS_2 0x3EF8
#define OFF_READ_POS_2 0x449C

static SceUID ctrl_hook_uid[CTRL_HOOKS] = {-1, -1, -1, -1, -1, -1};
static tai_hook_ref_t ctrl_hook_ref[CTRL_HOOKS];

static int stick_active = 0;
static int stick_center_x = 0;
static int stick_center_y = 0;
static unsigned char stick_rx = TOUCH_CENTER;
static unsigned char stick_ry = TOUCH_CENTER;

static int clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static void reset_virtual_stick(void) {
    stick_active = 0;
    stick_rx = TOUCH_CENTER;
    stick_ry = TOUCH_CENTER;
}

static void update_virtual_stick(void) {
    SceTouchData touch;
    int ret = ksceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);

    if (ret <= 0 || touch.reportNum == 0) {
        reset_virtual_stick();
        return;
    }

    const SceTouchReport *report = NULL;
    for (unsigned int i = 0; i < touch.reportNum; i++) {
        if (touch.report[i].x >= RIGHT_ZONE_MIN_X) {
            report = &touch.report[i];
            break;
        }
    }

    if (report == NULL) {
        reset_virtual_stick();
        return;
    }

    if (!stick_active) {
        stick_active = 1;
        stick_center_x = report->x;
        stick_center_y = report->y;
    }

    int dx = clamp_int(report->x - stick_center_x, -TOUCH_RADIUS, TOUCH_RADIUS);
    int dy = clamp_int(report->y - stick_center_y, -TOUCH_RADIUS, TOUCH_RADIUS);

    stick_rx = (unsigned char)clamp_int(TOUCH_CENTER + (dx * 127) / TOUCH_RADIUS, 0, 255);
    stick_ry = (unsigned char)clamp_int(TOUCH_CENTER + (dy * 127) / TOUCH_RADIUS, 0, 255);
}

static void patch_ctrl_data(SceCtrlData *pad_data, int count) {
    update_virtual_stick();
    for (int i = 0; i < count; i++) {
        pad_data[i].rx = stick_rx;
        pad_data[i].ry = stick_ry;
    }
}

#define DECL_CTRL_HOOK(name, index) \
static int name(int port, SceCtrlData *pad_data, int count) { \
    int ret = TAI_CONTINUE(int, ctrl_hook_ref[index], port, pad_data, count); \
    if (ret > 0) patch_ctrl_data(pad_data, ret); \
    return ret; \
}

DECL_CTRL_HOOK(ctrl_peek_positive_patched, 0)
DECL_CTRL_HOOK(ctrl_read_positive_patched, 1)
DECL_CTRL_HOOK(ctrl_peek_negative_patched, 2)
DECL_CTRL_HOOK(ctrl_read_negative_patched, 3)
DECL_CTRL_HOOK(ctrl_peek_positive2_patched, 4)
DECL_CTRL_HOOK(ctrl_read_positive2_patched, 5)

static void release_hooks(void) {
    for (int i = 0; i < CTRL_HOOKS; i++) {
        if (ctrl_hook_uid[i] >= 0) {
            taiHookReleaseForKernel(ctrl_hook_uid[i], ctrl_hook_ref[i]);
            ctrl_hook_uid[i] = -1;
        }
    }
}

int module_start(SceSize argc, const void *args) {
    (void)argc;
    (void)args;

    tai_module_info_t mod_info;
    mod_info.size = sizeof(tai_module_info_t);
    if (taiGetModuleInfoForKernel(KERNEL_PID, "SceCtrl", &mod_info) < 0)
        return SCE_KERNEL_START_FAILED;

    ctrl_hook_uid[0] = taiHookFunctionExportForKernel(KERNEL_PID, &ctrl_hook_ref[0], "SceCtrl", TAI_ANY_LIBRARY, NID_PEEK_POS, ctrl_peek_positive_patched);
    ctrl_hook_uid[1] = taiHookFunctionExportForKernel(KERNEL_PID, &ctrl_hook_ref[1], "SceCtrl", TAI_ANY_LIBRARY, NID_READ_POS, ctrl_read_positive_patched);
    ctrl_hook_uid[2] = taiHookFunctionExportForKernel(KERNEL_PID, &ctrl_hook_ref[2], "SceCtrl", TAI_ANY_LIBRARY, NID_PEEK_NEG, ctrl_peek_negative_patched);
    ctrl_hook_uid[3] = taiHookFunctionExportForKernel(KERNEL_PID, &ctrl_hook_ref[3], "SceCtrl", TAI_ANY_LIBRARY, NID_READ_NEG, ctrl_read_negative_patched);

    for (int i = 0; i < REQUIRED_HOOKS; i++) {
        if (ctrl_hook_uid[i] < 0) {
            release_hooks();
            return SCE_KERNEL_START_FAILED;
        }
    }

    ctrl_hook_uid[4] = taiHookFunctionOffsetForKernel(KERNEL_PID, &ctrl_hook_ref[4], mod_info.modid, 0, OFF_PEEK_POS_2 | 1, 1, ctrl_peek_positive2_patched);
    ctrl_hook_uid[5] = taiHookFunctionOffsetForKernel(KERNEL_PID, &ctrl_hook_ref[5], mod_info.modid, 0, OFF_READ_POS_2 | 1, 1, ctrl_read_positive2_patched);

    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
    (void)argc;
    (void)args;
    release_hooks();
    return SCE_KERNEL_STOP_SUCCESS;
}

void _start() __attribute__((weak, alias("module_start")));
