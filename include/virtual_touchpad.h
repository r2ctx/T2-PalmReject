// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Ryan C.

#ifndef VIRTUAL_TOUCHPAD_H
#define VIRTUAL_TOUCHPAD_H

int virtual_touchpad_create(void);

int virtual_touchpad_finger_down(int fd,
                                 int slot,
                                 int tracking_id,
                                 int x,
                                 int y,
                                 int touch_major,
                                 int width_major,
                                 int pressure);

int virtual_touchpad_finger_move(int fd,
                                 int slot,
                                 int x,
                                 int y,
                                 int touch_major,
                                 int width_major,
                                 int pressure);

int virtual_touchpad_finger_up(int fd, int slot);

int virtual_touchpad_set_finger_count(int fd, int count);

int virtual_touchpad_sync(int fd);

void virtual_touchpad_destroy(int fd);

#endif
