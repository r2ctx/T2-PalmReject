// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Ryan C.

#include "virtual_touchpad.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define TOUCHPAD_NAME "PalmReject Virtual Touchpad"

#define ABS_X_MIN -5318
#define ABS_X_MAX 5787
#define ABS_X_RES 91

#define ABS_Y_MIN -7102
#define ABS_Y_MAX 157
#define ABS_Y_RES 88

#define ABS_PRESSURE_MIN 0
#define ABS_PRESSURE_MAX 6000

#define ABS_TOUCH_SIZE_MIN 0
#define ABS_TOUCH_SIZE_MAX 5000

#define ABS_ORIENTATION_MIN -16384
#define ABS_ORIENTATION_MAX 16384

#define ABS_TRACKING_ID_MIN 0
#define ABS_TRACKING_ID_MAX 65535

#define ABS_SLOT_MIN 0
#define ABS_SLOT_MAX 15

static int emit_event(int fd, int type, int code, int value) {
    struct input_event event;

    memset(&event, 0, sizeof(event));
    event.type = type;
    event.code = code;
    event.value = value;

    if (write(fd, &event, sizeof(event)) != sizeof(event)) {
        printf("Could not write event: %s\n", strerror(errno));
        return 1;
    }

    return 0;
}

int virtual_touchpad_sync(int fd) {
    return emit_event(fd, EV_SYN, SYN_REPORT, 0);
}

static int setup_abs_axis(int fd,
                          int code,
                          int minimum,
                          int maximum,
                          int resolution) {
    struct uinput_abs_setup abs_setup;

    memset(&abs_setup, 0, sizeof(abs_setup));
    abs_setup.code = code;
    abs_setup.absinfo.minimum = minimum;
    abs_setup.absinfo.maximum = maximum;
    abs_setup.absinfo.resolution = resolution;

    if (ioctl(fd, UI_SET_ABSBIT, code) < 0) {
        printf("Could not enable ABS code %d: %s\n", code, strerror(errno));
        return 1;
    }

    if (ioctl(fd, UI_ABS_SETUP, &abs_setup) < 0) {
        printf("Could not setup ABS code %d: %s\n", code, strerror(errno));
        return 1;
    }

    return 0;
}

static void initialize_empty_slots(int fd) {
    for (int slot = ABS_SLOT_MIN; slot <= ABS_SLOT_MAX; slot++) {
        emit_event(fd, EV_ABS, ABS_MT_SLOT, slot);
        emit_event(fd, EV_ABS, ABS_MT_TRACKING_ID, -1);
    }

    emit_event(fd, EV_KEY, BTN_TOUCH, 0);
    emit_event(fd, EV_KEY, BTN_TOOL_FINGER, 0);
    emit_event(fd, EV_KEY, BTN_TOOL_DOUBLETAP, 0);
    emit_event(fd, EV_KEY, BTN_TOOL_TRIPLETAP, 0);
    emit_event(fd, EV_KEY, BTN_TOOL_QUADTAP, 0);
    emit_event(fd, EV_KEY, BTN_TOOL_QUINTTAP, 0);

    emit_event(fd, EV_ABS, ABS_PRESSURE, 0);
    emit_event(fd, EV_SYN, SYN_REPORT, 0);
}

int virtual_touchpad_create(void) {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

    if (fd < 0) {
        printf("Could not open /dev/uinput: %s\n", strerror(errno));
        return -1;
    }

    if (ioctl(fd, UI_SET_PROPBIT, INPUT_PROP_POINTER) < 0) {
        printf("Could not set INPUT_PROP_POINTER: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    if (ioctl(fd, UI_SET_PROPBIT, INPUT_PROP_BUTTONPAD) < 0) {
        printf("Could not set INPUT_PROP_BUTTONPAD: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) {
        printf("Could not enable EV_KEY: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    ioctl(fd, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH);
    ioctl(fd, UI_SET_KEYBIT, BTN_TOOL_FINGER);
    ioctl(fd, UI_SET_KEYBIT, BTN_TOOL_DOUBLETAP);
    ioctl(fd, UI_SET_KEYBIT, BTN_TOOL_TRIPLETAP);
    ioctl(fd, UI_SET_KEYBIT, BTN_TOOL_QUADTAP);
    ioctl(fd, UI_SET_KEYBIT, BTN_TOOL_QUINTTAP);

    if (ioctl(fd, UI_SET_EVBIT, EV_ABS) < 0) {
        printf("Could not enable EV_ABS: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    if (setup_abs_axis(fd, ABS_X, ABS_X_MIN, ABS_X_MAX, ABS_X_RES) != 0 ||
        setup_abs_axis(fd, ABS_Y, ABS_Y_MIN, ABS_Y_MAX, ABS_Y_RES) != 0 ||
        setup_abs_axis(fd, ABS_PRESSURE, ABS_PRESSURE_MIN, ABS_PRESSURE_MAX, 1) != 0 ||
        setup_abs_axis(fd, ABS_MT_SLOT, ABS_SLOT_MIN, ABS_SLOT_MAX, 0) != 0 ||
        setup_abs_axis(fd, ABS_MT_TOUCH_MAJOR, ABS_TOUCH_SIZE_MIN, ABS_TOUCH_SIZE_MAX, 0) != 0 ||
        setup_abs_axis(fd, ABS_MT_TOUCH_MINOR, ABS_TOUCH_SIZE_MIN, ABS_TOUCH_SIZE_MAX, 0) != 0 ||
        setup_abs_axis(fd, ABS_MT_WIDTH_MAJOR, ABS_TOUCH_SIZE_MIN, ABS_TOUCH_SIZE_MAX, 0) != 0 ||
        setup_abs_axis(fd, ABS_MT_WIDTH_MINOR, ABS_TOUCH_SIZE_MIN, ABS_TOUCH_SIZE_MAX, 0) != 0 ||
        setup_abs_axis(fd, ABS_MT_ORIENTATION, ABS_ORIENTATION_MIN, ABS_ORIENTATION_MAX, 0) != 0 ||
        setup_abs_axis(fd, ABS_MT_POSITION_X, ABS_X_MIN, ABS_X_MAX, ABS_X_RES) != 0 ||
        setup_abs_axis(fd, ABS_MT_POSITION_Y, ABS_Y_MIN, ABS_Y_MAX, ABS_Y_RES) != 0 ||
        setup_abs_axis(fd, ABS_MT_TRACKING_ID, ABS_TRACKING_ID_MIN, ABS_TRACKING_ID_MAX, 0) != 0 ||
        setup_abs_axis(fd, ABS_MT_PRESSURE, ABS_PRESSURE_MIN, ABS_PRESSURE_MAX, 1) != 0) {
        close(fd);
        return -1;
    }

    struct uinput_setup setup;
    memset(&setup, 0, sizeof(setup));

    snprintf(setup.name, UINPUT_MAX_NAME_SIZE, TOUCHPAD_NAME);
    setup.id.bustype = BUS_VIRTUAL;
    setup.id.vendor = 0x1234;
    setup.id.product = 0x5678;
    setup.id.version = 1;

    if (ioctl(fd, UI_DEV_SETUP, &setup) < 0) {
        printf("Could not setup virtual touchpad: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    if (ioctl(fd, UI_DEV_CREATE) < 0) {
        printf("Could not create virtual touchpad: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    usleep(300000);
    initialize_empty_slots(fd);
    usleep(700000);

    return fd;
}

int virtual_touchpad_set_finger_count(int fd, int count) {
    emit_event(fd, EV_KEY, BTN_TOUCH, count > 0);

    emit_event(fd, EV_KEY, BTN_TOOL_FINGER, count == 1);
    emit_event(fd, EV_KEY, BTN_TOOL_DOUBLETAP, count == 2);
    emit_event(fd, EV_KEY, BTN_TOOL_TRIPLETAP, count == 3);
    emit_event(fd, EV_KEY, BTN_TOOL_QUADTAP, count == 4);
    emit_event(fd, EV_KEY, BTN_TOOL_QUINTTAP, count >= 5);

    if (count == 0) {
        emit_event(fd, EV_ABS, ABS_PRESSURE, 0);
    }

    return 0;
}

int virtual_touchpad_finger_down(int fd,
                                 int slot,
                                 int tracking_id,
                                 int x,
                                 int y,
                                 int touch_major,
                                 int width_major,
                                 int pressure) {
    emit_event(fd, EV_ABS, ABS_MT_SLOT, slot);
    emit_event(fd, EV_ABS, ABS_MT_TRACKING_ID, tracking_id);
    emit_event(fd, EV_ABS, ABS_MT_POSITION_X, x);
    emit_event(fd, EV_ABS, ABS_MT_POSITION_Y, y);
    emit_event(fd, EV_ABS, ABS_MT_TOUCH_MAJOR, touch_major);
    emit_event(fd, EV_ABS, ABS_MT_TOUCH_MINOR, touch_major);
    emit_event(fd, EV_ABS, ABS_MT_WIDTH_MAJOR, width_major);
    emit_event(fd, EV_ABS, ABS_MT_WIDTH_MINOR, width_major);
    emit_event(fd, EV_ABS, ABS_MT_PRESSURE, pressure);

    emit_event(fd, EV_ABS, ABS_X, x);
    emit_event(fd, EV_ABS, ABS_Y, y);
    emit_event(fd, EV_ABS, ABS_PRESSURE, pressure);

    emit_event(fd, EV_KEY, BTN_TOUCH, 1);

    return 0;
}

int virtual_touchpad_finger_move(int fd,
                                 int slot,
                                 int x,
                                 int y,
                                 int touch_major,
                                 int width_major,
                                 int pressure) {
    emit_event(fd, EV_ABS, ABS_MT_SLOT, slot);
    emit_event(fd, EV_ABS, ABS_MT_POSITION_X, x);
    emit_event(fd, EV_ABS, ABS_MT_POSITION_Y, y);
    emit_event(fd, EV_ABS, ABS_MT_TOUCH_MAJOR, touch_major);
    emit_event(fd, EV_ABS, ABS_MT_TOUCH_MINOR, touch_major);
    emit_event(fd, EV_ABS, ABS_MT_WIDTH_MAJOR, width_major);
    emit_event(fd, EV_ABS, ABS_MT_WIDTH_MINOR, width_major);
    emit_event(fd, EV_ABS, ABS_MT_PRESSURE, pressure);

    if (slot == 0) {
        emit_event(fd, EV_ABS, ABS_X, x);
        emit_event(fd, EV_ABS, ABS_Y, y);
        emit_event(fd, EV_ABS, ABS_PRESSURE, pressure);
    }

    return 0;
}

int virtual_touchpad_finger_up(int fd, int slot) {
    emit_event(fd, EV_ABS, ABS_MT_SLOT, slot);
    emit_event(fd, EV_ABS, ABS_MT_TRACKING_ID, -1);

    return 0;
}

void virtual_touchpad_destroy(int fd) {
    if (fd >= 0) {
        ioctl(fd, UI_DEV_DESTROY);
        close(fd);
    }
}
