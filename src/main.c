// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Ryan C.

#include "virtual_touchpad.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define MAX_SLOTS 16

#define DEFAULT_TOUCH_MAJOR 120
#define DEFAULT_WIDTH_MAJOR 70
#define DEFAULT_PRESSURE 300

#define EDGE_X_THRESHOLD 4300
#define SIDE_X_THRESHOLD 5000
#define EXTREME_SIDE_X_THRESHOLD 5400

#define TOP_Y_THRESHOLD -5600
#define FAR_TOP_Y_THRESHOLD -6500

#define PALM_WIDTH_THRESHOLD 3000
#define PALM_TOUCH_THRESHOLD 850
#define EDGE_PALM_WIDTH_THRESHOLD 2500
#define EDGE_THIN_WIDTH_THRESHOLD 1700
#define TOP_STRIP_WIDTH_THRESHOLD 2200
#define COMBINED_WIDTH_THRESHOLD 2500
#define COMBINED_TOUCH_THRESHOLD 450


struct slot_state {
    int active;
    int virtual_active;

    int tracking_id;

    int x;
    int y;
    int have_x;
    int have_y;

    int touch_major;
    int width_major;
    int pressure;
};

static int physical_fd = -1;
static int virtual_fd = -1;

static struct slot_state slots[MAX_SLOTS];
static int current_slot = 0;

static int palm_lock_active = 0;

static int left_button_value = 0;
static int left_button_dirty = 0;

static void cleanup(void) {
    if (physical_fd >= 0) {
        ioctl(physical_fd, EVIOCGRAB, 0);
        close(physical_fd);
        physical_fd = -1;
    }

    if (virtual_fd >= 0) {
        virtual_touchpad_destroy(virtual_fd);
        virtual_fd = -1;
    }
}

static int emit_raw_event(int fd, int type, int code, int value) {
    struct input_event event;

    memset(&event, 0, sizeof(event));
    event.type = type;
    event.code = code;
    event.value = value;

    if (write(fd, &event, sizeof(event)) != sizeof(event)) {
        printf("Could not write raw event: %s\n", strerror(errno));
        return 1;
    }

    return 0;
}

static void handle_signal(int signal_number) {
    (void)signal_number;
    printf("\nCleaning up\n");
    cleanup();
    _exit(0);
}

static void reset_slots(void) {
    for (int i = 0; i < MAX_SLOTS; i++) {
        slots[i].active = 0;
        slots[i].virtual_active = 0;

        slots[i].tracking_id = -1;

        slots[i].x = 0;
        slots[i].y = 0;
        slots[i].have_x = 0;
        slots[i].have_y = 0;

        slots[i].touch_major = DEFAULT_TOUCH_MAJOR;
        slots[i].width_major = DEFAULT_WIDTH_MAJOR;
        slots[i].pressure = DEFAULT_PRESSURE;
    }
}

static int active_slot_count(void) {
    int count = 0;

    for (int i = 0; i < MAX_SLOTS; i++) {
        if (slots[i].active && slots[i].have_x && slots[i].have_y) {
            count++;
        }
    }

    return count;
}

static int safe_touch_major(int value) {
    if (value <= 0) {
        return DEFAULT_TOUCH_MAJOR;
    }

    return value;
}

static int safe_width_major(int value) {
    if (value <= 0) {
        return DEFAULT_WIDTH_MAJOR;
    }

    return value;
}

static int safe_pressure(int value) {
    if (value <= 0) {
        return DEFAULT_PRESSURE;
    }

    return value;
}


static int is_palm_shape(int x,
                         int y,
                         int touch_major,
                         int width_major,
                         int finger_count) {
    int abs_x = abs(x);
    int near_edge = abs_x > EDGE_X_THRESHOLD;
    int near_side = abs_x > SIDE_X_THRESHOLD;
    int extreme_side = abs_x > EXTREME_SIDE_X_THRESHOLD;
    int upper_area = y < TOP_Y_THRESHOLD;
    int far_upper_area = y < FAR_TOP_Y_THRESHOLD;

    return width_major > PALM_WIDTH_THRESHOLD ||
           touch_major > PALM_TOUCH_THRESHOLD ||

           (near_edge && width_major > EDGE_PALM_WIDTH_THRESHOLD) ||

           (width_major > COMBINED_WIDTH_THRESHOLD &&
            touch_major > COMBINED_TOUCH_THRESHOLD) ||

           (near_side &&
            upper_area &&
            width_major > EDGE_THIN_WIDTH_THRESHOLD) ||

           (extreme_side &&
            width_major > EDGE_THIN_WIDTH_THRESHOLD) ||

           (far_upper_area &&
            width_major > TOP_STRIP_WIDTH_THRESHOLD) ||

           (finger_count >= 2 &&
            upper_area &&
            width_major > TOP_STRIP_WIDTH_THRESHOLD) ||

           (finger_count >= 2 &&
            near_side &&
            width_major > EDGE_THIN_WIDTH_THRESHOLD);
}

static int frame_has_palm_candidate(void) {
    int raw_count = active_slot_count();

    for (int i = 0; i < MAX_SLOTS; i++) {
        if (slots[i].active && slots[i].have_x && slots[i].have_y) {
            int touch_major = safe_touch_major(slots[i].touch_major);
            int width_major = safe_width_major(slots[i].width_major);

            if (is_palm_shape(slots[i].x,
                              slots[i].y,
                              touch_major,
                              width_major,
                              raw_count)) {
                return 1;
            }
        }
    }

    return 0;
}

static void release_all_virtual_fingers(void) {
    for (int i = 0; i < MAX_SLOTS; i++) {
        if (slots[i].virtual_active) {
            virtual_touchpad_finger_up(virtual_fd, i);
            slots[i].virtual_active = 0;
        }
    }

    virtual_touchpad_set_finger_count(virtual_fd, 0);
}


static void emit_clean_frame(void) {
    int count = active_slot_count();
    int palm_now = frame_has_palm_candidate();

    if (palm_now && !palm_lock_active) {
        palm_lock_active = 1;
    }

    if (palm_lock_active) {
        release_all_virtual_fingers();

        if (count == 0) {
            palm_lock_active = 0;
        }

        virtual_touchpad_sync(virtual_fd);
        return;
    }

    for (int i = 0; i < MAX_SLOTS; i++) {
        if (!slots[i].active && slots[i].virtual_active) {
            virtual_touchpad_finger_up(virtual_fd, i);
            slots[i].virtual_active = 0;
        }
    }

    for (int i = 0; i < MAX_SLOTS; i++) {
        if (slots[i].active && slots[i].have_x && slots[i].have_y) {
            int touch_major = safe_touch_major(slots[i].touch_major);
            int width_major = safe_width_major(slots[i].width_major);
            int pressure = safe_pressure(slots[i].pressure);

            if (!slots[i].virtual_active) {
                virtual_touchpad_finger_down(virtual_fd,
                                             i,
                                             slots[i].tracking_id,
                                             slots[i].x,
                                             slots[i].y,
                                             touch_major,
                                             width_major,
                                             pressure);

                slots[i].virtual_active = 1;
            } else {
                virtual_touchpad_finger_move(virtual_fd,
                                             i,
                                             slots[i].x,
                                             slots[i].y,
                                             touch_major,
                                             width_major,
                                             pressure);
            }
        }
    }

    virtual_touchpad_set_finger_count(virtual_fd, count);

    if (left_button_dirty) {
        emit_raw_event(virtual_fd, EV_KEY, BTN_LEFT, left_button_value);
        left_button_dirty = 0;
    }

    virtual_touchpad_sync(virtual_fd);
}

static void handle_abs_event(struct input_event *event) {
    switch (event->code) {
        case ABS_MT_SLOT:
            if (event->value >= 0 && event->value < MAX_SLOTS) {
                current_slot = event->value;
            }
            break;

        case ABS_MT_TRACKING_ID:
            if (current_slot >= 0 && current_slot < MAX_SLOTS) {
                if (event->value < 0) {
                    slots[current_slot].active = 0;
                    slots[current_slot].tracking_id = -1;
                    slots[current_slot].have_x = 0;
                    slots[current_slot].have_y = 0;
                } else {
                    slots[current_slot].active = 1;
                    slots[current_slot].virtual_active = 0;
                    slots[current_slot].tracking_id = event->value;
                    slots[current_slot].have_x = 0;
                    slots[current_slot].have_y = 0;
                    slots[current_slot].touch_major = DEFAULT_TOUCH_MAJOR;
                    slots[current_slot].width_major = DEFAULT_WIDTH_MAJOR;
                    slots[current_slot].pressure = DEFAULT_PRESSURE;
                }
            }
            break;

        case ABS_MT_POSITION_X:
            if (current_slot >= 0 && current_slot < MAX_SLOTS) {
                slots[current_slot].x = event->value;
                slots[current_slot].have_x = 1;
            }
            break;

        case ABS_MT_POSITION_Y:
            if (current_slot >= 0 && current_slot < MAX_SLOTS) {
                slots[current_slot].y = event->value;
                slots[current_slot].have_y = 1;
            }
            break;

        case ABS_MT_TOUCH_MAJOR:
            if (current_slot >= 0 && current_slot < MAX_SLOTS) {
                slots[current_slot].touch_major = event->value;
            }
            break;

        case ABS_MT_WIDTH_MAJOR:
            if (current_slot >= 0 && current_slot < MAX_SLOTS) {
                slots[current_slot].width_major = event->value;
            }
            break;

        case ABS_MT_PRESSURE:
            if (current_slot >= 0 && current_slot < MAX_SLOTS) {
                slots[current_slot].pressure = event->value;
            }
            break;

        default:
            break;
    }
}

static void handle_key_event(struct input_event *event) {
    if (event->code == BTN_LEFT) {
        left_button_value = event->value;
        left_button_dirty = 1;
    }
}

int main(int argc, char **argv) {
    const char *physical_path = "/dev/input/event7";

    if (argc >= 2) {
        physical_path = argv[1];
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    reset_slots();

    physical_fd = open(physical_path, O_RDONLY);

    if (physical_fd < 0) {
        printf("Could not open %s: %s\n", physical_path, strerror(errno));
        return 1;
    }

    virtual_fd = virtual_touchpad_create();

    if (virtual_fd < 0) {
        printf("Could not create virtual touchpad\n");
        cleanup();
        return 1;
    }

    if (ioctl(physical_fd, EVIOCGRAB, 1) < 0) {
        printf("Could not grab %s: %s\n", physical_path, strerror(errno));
        cleanup();
        return 1;
    }

    while (1) {
        struct input_event event;
        ssize_t bytes_read = read(physical_fd, &event, sizeof(event));

        if (bytes_read != sizeof(event)) {
            printf("Could not read event: %s\n", strerror(errno));
            cleanup();
            return 1;
        }

        if (event.type == EV_ABS) {
            handle_abs_event(&event);
        } else if (event.type == EV_KEY) {
            handle_key_event(&event);
        } else if (event.type == EV_SYN && event.code == SYN_REPORT) {
            emit_clean_frame();
        }
    }

    cleanup();
    return 0;
}
