# T2 PalmReject

T2 PalmReject is a lightweight userspace palm-rejection filter for the Apple
internal keyboard/trackpad found on a tested Intel T2 MacBook running Linux.
It grabs the physical trackpad, filters palm-shaped contacts, and forwards
accepted contacts through a virtual multitouch touchpad so libinput keeps
handling pointer acceleration, two-finger scrolling, clicking, and gestures.

## Current support

This release is intentionally narrow.

- Tested target: 2020 Intel MacBook Air with T2 Linux
- Distribution: Arch-based Linux
- Desktop tested: Hyprland
- Input stack: evdev, uinput, and libinput
- Service manager: systemd

The architecture is not inherently Hyprland-specific, but the current axis
ranges and palm thresholds are calibrated for the tested Apple trackpad. Do
not advertise this release as a universal Linux palm-rejection tool.

## Safety warning

PalmReject exclusively grabs the real trackpad while it is running. If the
virtual touchpad fails, pointer input can appear to stop.

Emergency recovery from a terminal, TTY, or external mouse:

```bash
sudo systemctl stop palmreject.service
```

To remove it completely:

```bash
sudo ./uninstall.sh
```

## Requirements

Install the standard Arch build tools and libinput utilities:

```bash
sudo pacman -S --needed base-devel libinput
```

`brightnessctl` is optional. When present and the tested keyboard-backlight
device is detected, the installer can restore the keyboard backlight after
resume.

## Installation

Extract the archive and enter the directory:

```bash
tar -xzf T2_PalmReject-v1.0.0.tar.gz
cd T2_PalmReject-v1.0.0
```

First run a non-destructive check:

```bash
./install.sh --dry-run
```

Then install:

```bash
sudo ./install.sh
```

The installer:

1. Builds `palmreject` from source.
2. Detects the Apple internal trackpad using stable `/dev/input/by-id` or
   `/dev/input/by-path` links.
3. Writes `/etc/palmreject.conf`.
4. Installs `/usr/local/bin/palmreject`.
5. Installs and enables `palmreject.service`.
6. Installs a suspend/resume hook that stops the service before sleep and
   restarts it after wake.
7. Loads `uinput` and configures it to load at boot.

To choose the device manually:

```bash
sudo ./install.sh --device /dev/input/by-id/YOUR-TRACKPAD-event-mouse
```

To skip keyboard-backlight restoration:

```bash
sudo ./install.sh --no-backlight
```

## Service commands

```bash
systemctl status palmreject.service
sudo systemctl restart palmreject.service
sudo systemctl stop palmreject.service
journalctl -u palmreject.service -b
```

## Uninstall

From the source directory:

```bash
sudo ./uninstall.sh
```

Preserve `/etc/palmreject.conf` while removing everything else:

```bash
sudo ./uninstall.sh --keep-config
```

## Manual build

```bash
make
sudo ./palmreject /dev/input/by-id/YOUR-TRACKPAD-event-mouse
```

Stop the manual process with `Ctrl+C` before starting the systemd service.

## Project layout

```text
.
├── include/
│   └── virtual_touchpad.h
├── scripts/
│   └── detect-trackpad.sh
├── src/
│   ├── main.c
│   └── virtual_touchpad.c
├── systemd/
│   ├── palmreject.service
│   └── palmreject-sleep
├── install.sh
├── uninstall.sh
├── Makefile
├── VERSION
├── LICENSE
└── README.md
```

## Known limitations

- Coordinate ranges and palm thresholds are hardcoded for the tested Apple
  trackpad.
- Thumb support is limited and will be improved in a later classifier.
- Device detection targets the Apple internal keyboard/trackpad name.
- This release has not yet been validated across other MacBook models or
  non-Arch distributions.

## Architecture

```text
Physical Apple trackpad
        |
        | EVIOCGRAB
        v
T2 PalmReject classifier
        |
        | fingers forwarded; palms globally blocked
        v
Virtual multitouch touchpad
        |
        v
libinput -> compositor/desktop
```

## License

T2 PalmReject is licensed under the GNU General Public License v3.0 only.
See `LICENSE` for the complete terms.

Copyright (C) 2026 Ryan C.

Developed by Ryan C. with assistance from ChatGPT during prototyping and
packaging.
