#include <vitasdkkern.h>
#include <psp2kern/ctrl.h>
#include <psp2/touch.h>
#include <taihen.h>

/*
 * VitaSDK currently exposes the touch data structures/constants in the user
 * header, while SceTouchForDriver still provides the kernel driver import.
 * This prototype mirrors the approach used by reVita's VitaSDK extensions.
 */
int ksceTouchPeek(SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs);

/*
 * VirtualVitaRAStick - first functional prototype
 *
 * The right half of the front touchscreen acts as a floating right analog
 * stick. The first touch becomes the stick center; movement around that point
 * is mapped to rx/ry. Releasing the finger returns the stick to center.
 */

#define CTRL_HOOKS 2
#define TOUCH_CENTER 128
#define TOUCH_RADIUS 320
#define RIGHT_ZONE_MIN_X 960

static SceUID ctrl_hook_uid[CTRL_HOOKS] = {-1, -1};
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

static void update_virtual_stick(void) {
    SceTouchData touch;
    int ret = ksceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);

    if (ret <= 0 || touch.reportNum == 0) {
        stick_active = 0;
        stick_rx = TOUCH_CENTER;
        stick_ry = TOUCH_CENTER;
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
        stick_active = 0;
        stick_rx = TOUCH_CENTER;
        stick_ry = TOUCH_CENTER;
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

static int ctrl_peek_positive_patched(int port, SceCtrlData *pad_data, int count) {
    int ret = TAI_CONTINUE(int, ctrl_hook_ref[0], port, pad_data, count);
    if (ret > 0) patch_ctrl_data(pad_data, ret);
    return ret;
}

static int ctrl_read_positive_patched(int port, SceCtrlData *pad_data, int count) {
    int ret = TAI_CONTINUE(int, ctrl_hook_ref[1], port, pad_data, count);
    if (ret > 0) patch_ctrl_data(pad_data, ret);
    return ret;
}

int module_start(SceSize argc, const void *args) {
    (void)argc;
    (void)args;

    ctrl_hook_uid[0] = taiHookFunctionExportForKernel(
        KERNEL_PID,
        &ctrl_hook_ref[0],
        "SceCtrl",
        TAI_ANY_LIBRARY,
        0xA9C3CED6,
        ctrl_peek_positive_patched
    );

    ctrl_hook_uid[1] = taiHookFunctionExportForKernel(
        KERNEL_PID,
        &ctrl_hook_ref[1],
        "SceCtrl",
        TAI_ANY_LIBRARY,
        0x67E7AB83,
        ctrl_read_positive_patched
    );

    if (ctrl_hook_uid[0] < 0 || ctrl_hook_uid[1] < 0) {
        if (ctrl_hook_uid[0] >= 0)
            taiHookReleaseForKernel(ctrl_hook_uid[0], ctrl_hook_ref[0]);
        if (ctrl_hook_uid[1] >= 0)
            taiHookReleaseForKernel(ctrl_hook_uid[1], ctrl_hook_ref[1]);
        return SCE_KERNEL_START_FAILED;
    }

    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
    (void)argc;
    (void)args;

    for (int i = 0; i < CTRL_HOOKS; i++) {
        if (ctrl_hook_uid[i] >= 0)
            taiHookReleaseForKernel(ctrl_hook_uid[i], ctrl_hook_ref[i]);
    }

    return SCE_KERNEL_STOP_SUCCESS;
}

void _start() __attribute__((weak, alias("module_start")));
