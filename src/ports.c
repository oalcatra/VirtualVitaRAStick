#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <taihen.h>

#define TOUCH_CENTER 128
#define TOUCH_RADIUS 180
#define RIGHT_ZONE_MIN_X 960
#define BOTTOM_ZONE_MIN_Y 544
#define NID_PEEK_POSITIVE_EXT2 0x860BF292
#define NID_READ_POSITIVE_EXT2 0xA7178860

static SceUID hook_uid[2] = {-1, -1};
static tai_hook_ref_t hook_ref[2];
static int stick_active = 0;
static int stick_center_x = 0;
static int stick_center_y = 0;

static int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void apply_virtual_stick(SceCtrlData *pad_data, int count) {
    if (!pad_data || count <= 0) return;

    SceTouchData touch;
    int tret = sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);
    const SceTouchReport *report = NULL;

    if (tret > 0) {
        for (unsigned int i = 0; i < touch.reportNum; i++) {
            if (touch.report[i].x >= RIGHT_ZONE_MIN_X &&
                touch.report[i].y >= BOTTOM_ZONE_MIN_Y) {
                report = &touch.report[i];
                break;
            }
        }
    }

    unsigned char rx = TOUCH_CENTER;
    unsigned char ry = TOUCH_CENTER;

    if (report) {
        if (!stick_active) {
            stick_active = 1;
            stick_center_x = report->x;
            stick_center_y = report->y;
        }
        int dx = clamp_int(report->x - stick_center_x, -TOUCH_RADIUS, TOUCH_RADIUS);
        int dy = clamp_int(report->y - stick_center_y, -TOUCH_RADIUS, TOUCH_RADIUS);
        rx = (unsigned char)clamp_int(TOUCH_CENTER + (dx * 127) / TOUCH_RADIUS, 0, 255);
        ry = (unsigned char)clamp_int(TOUCH_CENTER + (dy * 127) / TOUCH_RADIUS, 0, 255);
    } else {
        stick_active = 0;
    }

    for (int i = 0; i < count; i++) {
        pad_data[i].rx = rx;
        pad_data[i].ry = ry;
    }
}

static int peek_ext2_hook(int port, SceCtrlData *pad_data, int count) {
    int ret = TAI_CONTINUE(int, hook_ref[0], port, pad_data, count);
    if (ret > 0 && port == 0) apply_virtual_stick(pad_data, ret);
    return ret;
}

static int read_ext2_hook(int port, SceCtrlData *pad_data, int count) {
    int ret = TAI_CONTINUE(int, hook_ref[1], port, pad_data, count);
    if (ret > 0 && port == 0) apply_virtual_stick(pad_data, ret);
    return ret;
}

int module_start(SceSize argc, const void *args) {
    (void)argc; (void)args;
    hook_uid[0] = taiHookFunctionImport(&hook_ref[0], TAI_MAIN_MODULE, TAI_ANY_LIBRARY,
                                        NID_PEEK_POSITIVE_EXT2, peek_ext2_hook);
    hook_uid[1] = taiHookFunctionImport(&hook_ref[1], TAI_MAIN_MODULE, TAI_ANY_LIBRARY,
                                        NID_READ_POSITIVE_EXT2, read_ext2_hook);
    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
    (void)argc; (void)args;
    for (int i = 0; i < 2; i++) {
        if (hook_uid[i] >= 0) taiHookRelease(hook_uid[i], hook_ref[i]);
    }
    return SCE_KERNEL_STOP_SUCCESS;
}

void _start() __attribute__((weak, alias("module_start")));
