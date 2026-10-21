#include <vitasdkkern.h>
#include <psp2kern/ctrl.h>
#include <psp2/touch.h>
#include <taihen.h>

int ksceTouchPeek(SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs);

#define TOUCH_CENTER 128
#define TOUCH_RADIUS 180
#define RIGHT_ZONE_MIN_X 960

#define NID_PEEK_POSITIVE 0xEA1D3A34
#define NID_READ_POSITIVE 0x9B96A1AA
#define OFF_PEEK_POSITIVE2 0x3EF8
#define OFF_READ_POSITIVE2 0x449C

static SceUID hook_uid[4] = {-1, -1, -1, -1};
static tai_hook_ref_t hook_ref[4];
static SceUID touch_thread_uid = -1;
static volatile int touch_thread_run = 0;

static int stick_active = 0;
static int stick_center_x = 0;
static int stick_center_y = 0;
static volatile unsigned char stick_rx = TOUCH_CENTER;
static volatile unsigned char stick_ry = TOUCH_CENTER;

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

/* Sample touch independently from SceCtrl. This lets games that call the
 * internal Positive2 path directly receive fresh touch state, while avoiding
 * multiple touch reads when one SceCtrl path calls another internally. */
static int touch_thread(SceSize args, void *argp) {
    (void)args;
    (void)argp;

    while (touch_thread_run) {
        update_virtual_stick();
        ksceKernelDelayThread(2000);
    }

    return 0;
}

static void write_virtual_stick(SceCtrlData *pad_data, int count) {
    if (pad_data == NULL || count <= 0)
        return;

    unsigned char rx = stick_rx;
    unsigned char ry = stick_ry;

    for (int i = 0; i < count; i++) {
        pad_data[i].rx = rx;
        pad_data[i].ry = ry;
    }
}

static int peek_positive_hook(int port, SceCtrlData *pad_data, int count) {
    int ret = TAI_CONTINUE(int, hook_ref[0], port, pad_data, count);
    if (ret > 0) write_virtual_stick(pad_data, ret);
    return ret;
}

static int read_positive_hook(int port, SceCtrlData *pad_data, int count) {
    int ret = TAI_CONTINUE(int, hook_ref[1], port, pad_data, count);
    if (ret > 0) write_virtual_stick(pad_data, ret);
    return ret;
}

static int peek_positive2_hook(int port, SceCtrlData *pad_data, int count) {
    int ret = TAI_CONTINUE(int, hook_ref[2], port, pad_data, count, 1);
    if (ret > 0) write_virtual_stick(pad_data, ret);
    return ret;
}

static int read_positive2_hook(int port, SceCtrlData *pad_data, int count) {
    int ret = TAI_CONTINUE(int, hook_ref[3], port, pad_data, count, 1);
    if (ret > 0) write_virtual_stick(pad_data, ret);
    return ret;
}

static void release_hooks(void) {
    for (int i = 0; i < 4; i++) {
        if (hook_uid[i] >= 0) {
            taiHookReleaseForKernel(hook_uid[i], hook_ref[i]);
            hook_uid[i] = -1;
        }
    }
}

static void stop_touch_thread(void) {
    if (touch_thread_uid >= 0) {
        touch_thread_run = 0;
        ksceKernelWaitThreadEnd(touch_thread_uid, NULL, NULL);
        ksceKernelDeleteThread(touch_thread_uid);
        touch_thread_uid = -1;
    }
}

int module_start(SceSize argc, const void *args) {
    (void)argc;
    (void)args;

    reset_virtual_stick();

    hook_uid[0] = taiHookFunctionExportForKernel(
        KERNEL_PID, &hook_ref[0], "SceCtrl", TAI_ANY_LIBRARY,
        NID_PEEK_POSITIVE, peek_positive_hook);

    hook_uid[1] = taiHookFunctionExportForKernel(
        KERNEL_PID, &hook_ref[1], "SceCtrl", TAI_ANY_LIBRARY,
        NID_READ_POSITIVE, read_positive_hook);

    if (hook_uid[0] < 0 || hook_uid[1] < 0) {
        release_hooks();
        return SCE_KERNEL_START_FAILED;
    }

    tai_module_info_t ctrl_info;
    ctrl_info.size = sizeof(ctrl_info);

    if (taiGetModuleInfoForKernel(KERNEL_PID, "SceCtrl", &ctrl_info) >= 0) {
        hook_uid[2] = taiHookFunctionOffsetForKernel(
            KERNEL_PID, &hook_ref[2], ctrl_info.modid, 0,
            OFF_PEEK_POSITIVE2, 1, peek_positive2_hook);

        hook_uid[3] = taiHookFunctionOffsetForKernel(
            KERNEL_PID, &hook_ref[3], ctrl_info.modid, 0,
            OFF_READ_POSITIVE2, 1, read_positive2_hook);
    }

    touch_thread_uid = ksceKernelCreateThread(
        "VirtualVitaRAStickTouch", touch_thread,
        0x40, 0x1000, 0, 0, NULL);

    if (touch_thread_uid < 0) {
        release_hooks();
        return SCE_KERNEL_START_FAILED;
    }

    touch_thread_run = 1;
    if (ksceKernelStartThread(touch_thread_uid, 0, NULL) < 0) {
        touch_thread_run = 0;
        ksceKernelDeleteThread(touch_thread_uid);
        touch_thread_uid = -1;
        release_hooks();
        return SCE_KERNEL_START_FAILED;
    }

    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
    (void)argc;
    (void)args;
    stop_touch_thread();
    release_hooks();
    return SCE_KERNEL_STOP_SUCCESS;
}

void _start() __attribute__((weak, alias("module_start")));
