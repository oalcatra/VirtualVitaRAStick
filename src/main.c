#include <vitasdkkern.h>
#include <psp2kern/ctrl.h>
#include <psp2/touch.h>

int ksceTouchPeek(SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs);

#define TOUCH_CENTER 128
#define TOUCH_RADIUS 180
#define RIGHT_ZONE_MIN_X 960
#define ANALOG_EMU_SLOT 0
#define ANALOG_EMU_DURATION 2

static int stick_active = 0;
static int stick_center_x = 0;
static int stick_center_y = 0;
static unsigned char stick_rx = TOUCH_CENTER;
static unsigned char stick_ry = TOUCH_CENTER;
static SceUID worker_uid = -1;
static volatile int worker_run = 1;

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

static void apply_analog_emulation(void) {
    /* Keep the physical left stick untouched by mirroring its neutral value in
     * the emulation fields. The right pair is supplied for both user and kernel
     * consumers so games using different SceCtrl read paths see the same stick. */
    ksceCtrlSetAnalogEmulation(
        0,
        ANALOG_EMU_SLOT,
        TOUCH_CENTER, TOUCH_CENTER, stick_rx, stick_ry,
        TOUCH_CENTER, TOUCH_CENTER, stick_rx, stick_ry,
        ANALOG_EMU_DURATION
    );
}

static int analog_worker(SceSize args, void *argp) {
    (void)args;
    (void)argp;

    while (worker_run) {
        update_virtual_stick();
        apply_analog_emulation();
        ksceKernelDelayThread(4000);
    }

    return 0;
}

int module_start(SceSize argc, const void *args) {
    (void)argc;
    (void)args;

    worker_run = 1;
    reset_virtual_stick();

    worker_uid = ksceKernelCreateThread(
        "VirtualVitaRAStick",
        analog_worker,
        0x60,
        0x1000,
        0,
        0,
        NULL
    );

    if (worker_uid < 0)
        return SCE_KERNEL_START_FAILED;

    int ret = ksceKernelStartThread(worker_uid, 0, NULL);
    if (ret < 0) {
        ksceKernelDeleteThread(worker_uid);
        worker_uid = -1;
        return SCE_KERNEL_START_FAILED;
    }

    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
    (void)argc;
    (void)args;

    worker_run = 0;

    if (worker_uid >= 0) {
        ksceKernelWaitThreadEnd(worker_uid, NULL, NULL);
        ksceKernelDeleteThread(worker_uid);
        worker_uid = -1;
    }

    reset_virtual_stick();
    apply_analog_emulation();

    return SCE_KERNEL_STOP_SUCCESS;
}

void _start() __attribute__((weak, alias("module_start")));
