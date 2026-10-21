#include <vitasdkkern.h>
#include <psp2kern/ctrl.h>
#include <psp2/touch.h>
#include <taihen.h>

/* VitaSDK exposes touch structures/constants in the user header while the
 * SceTouchForDriver stub provides the kernel import. This mirrors reVita. */
int ksceTouchPeek(SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs);

/*
 * VirtualVitaRAStick
 *
 * The right half of the front touchscreen acts as a floating right analog
 * stick. The first touch becomes the stick center; movement around that point
 * is mapped to rx/ry. Releasing the finger returns the stick to center.
 *
 * The physical right stick is intentionally overridden. When no qualifying
 * front-touch input is active, rx/ry are forced to the neutral value 128.
 * This prevents drift/noise from a broken or physically missing right stick.
 *
 * Coexistence note: reVita hooks these same SceCtrl exports. We always call
 * TAI_CONTINUE first and alter only rx/ry afterwards, keeping every other
 * controller field exactly as produced by the rest of the hook chain.
 */

#define CTRL_HOOKS 2
#define TOUCH_CENTER 128
#define TOUCH_RADIUS 180
#define RIGHT_ZONE_MIN_X 960

/* SceCtrlForDriver export NIDs used by reVita for the kernel positive
 * controller functions. */
#define NID_KSCE_CTRL_PEEK_BUFFER_POSITIVE 0xEA1D3A34
#define NID_KSCE_CTRL_READ_BUFFER_POSITIVE 0x9B96A1AA

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

static void reset_virtual_stick(void) {
    stick_active = 0;
    stick_rx = TOUCH_CENTER;
    stick_ry = TOUCH_CENTER;
}

static int update_virtual_stick(void) {
    SceTouchData touch;
    int ret = ksceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);

    if (ret <= 0 || touch.reportNum == 0) {
        reset_virtual_stick();
        return 0;
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
        return 0;
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
    return 1;
}

static void patch_ctrl_data(SceCtrlData *pad_data, int count) {
    /* Always own the right-stick axes. If touch is idle, update_virtual_stick()
     * resets them to 128/128, neutralizing any drift/noise from a missing or
     * broken physical right stick. While touch is active, stick_rx/stick_ry
     * contain the virtual-stick position. */
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
        NID_KSCE_CTRL_PEEK_BUFFER_POSITIVE,
        ctrl_peek_positive_patched
    );

    ctrl_hook_uid[1] = taiHookFunctionExportForKernel(
        KERNEL_PID,
        &ctrl_hook_ref[1],
        "SceCtrl",
        TAI_ANY_LIBRARY,
        NID_KSCE_CTRL_READ_BUFFER_POSITIVE,
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
