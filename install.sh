#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
DETECT_SCRIPT="$ROOT_DIR/scripts/detect-trackpad.sh"
SERVICE_SOURCE="$ROOT_DIR/systemd/palmreject.service"
SLEEP_SOURCE="$ROOT_DIR/systemd/palmreject-sleep"

DEVICE_OVERRIDE=""
ASSUME_YES=0
DRY_RUN=0
FORCE_UNSUPPORTED=0
BACKLIGHT_MODE="auto"
BACKLIGHT_DEVICE=":white:kbd_backlight"
BACKLIGHT_LEVEL="2%"
RESUME_DELAY="2"

usage() {
    cat <<'USAGE'
Usage: ./install.sh [options]

Build and install T2 PalmReject, detect the Apple internal trackpad, create
its configuration, enable the systemd service, and install the suspend/resume
hook.

Options:
  --device PATH          Use this input device instead of auto-detection.
  --yes                  Accept detected settings without prompting.
  --dry-run              Build and show the installation plan only.
  --no-backlight         Do not restore keyboard backlight after resume.
  --backlight-device ID  brightnessctl device (default: :white:kbd_backlight).
  --backlight-level VAL  Resume brightness (default: 2%).
  --resume-delay SEC     Delay before restarting after resume (default: 2).
  --force                Continue on a non-Arch distribution.
  -h, --help             Show this help.
USAGE
}

log() {
    printf '[PalmReject] %s\n' "$*"
}

warn() {
    printf '[PalmReject] WARNING: %s\n' "$*" >&2
}

die() {
    printf '[PalmReject] ERROR: %s\n' "$*" >&2
    exit 1
}

confirm() {
    local prompt="$1"
    local reply

    if (( ASSUME_YES )); then
        return 0
    fi

    read -r -p "$prompt [Y/n] " reply
    [[ -z "$reply" || "$reply" =~ ^[Yy]$ ]]
}

while (( $# > 0 )); do
    case "$1" in
        --device)
            [[ $# -ge 2 ]] || die "--device requires a path."
            DEVICE_OVERRIDE="$2"
            shift 2
            ;;
        --yes)
            ASSUME_YES=1
            shift
            ;;
        --dry-run)
            DRY_RUN=1
            shift
            ;;
        --no-backlight)
            BACKLIGHT_MODE="off"
            shift
            ;;
        --backlight-device)
            [[ $# -ge 2 ]] || die "--backlight-device requires a value."
            BACKLIGHT_DEVICE="$2"
            shift 2
            ;;
        --backlight-level)
            [[ $# -ge 2 ]] || die "--backlight-level requires a value."
            BACKLIGHT_LEVEL="$2"
            shift 2
            ;;
        --resume-delay)
            [[ $# -ge 2 ]] || die "--resume-delay requires seconds."
            RESUME_DELAY="$2"
            shift 2
            ;;
        --force)
            FORCE_UNSUPPORTED=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            usage >&2
            die "Unknown option: $1"
            ;;
    esac
done

if [[ ! "$RESUME_DELAY" =~ ^[0-9]+([.][0-9]+)?$ ]]; then
    die "Resume delay must be a non-negative number."
fi

if [[ ! -f /etc/arch-release ]]; then
    if (( ! FORCE_UNSUPPORTED )); then
        die "This release is supported on Arch-based systems. Use --force to continue anyway."
    fi
    warn "Continuing on an unsupported distribution."
fi

for command in make readlink systemctl install; do
    command -v "$command" >/dev/null 2>&1 || die "Required command not found: $command"
done

if command -v cc >/dev/null 2>&1; then
    export CC="${CC:-cc}"
elif command -v gcc >/dev/null 2>&1; then
    export CC="${CC:-gcc}"
else
    die "No C compiler found. Install the Arch base-devel package group."
fi

if [[ -n "$DEVICE_OVERRIDE" ]]; then
    DEVICE_PATH="$DEVICE_OVERRIDE"
else
    DEVICE_PATH="$($DETECT_SCRIPT)" || die \
        "Trackpad auto-detection failed. Re-run with --device /dev/input/by-id/..."
fi

[[ "$DEVICE_PATH" == /* ]] || die "Input device path must be absolute: $DEVICE_PATH"

DEVICE_TARGET="$(readlink -f -- "$DEVICE_PATH" 2>/dev/null || true)"
[[ -n "$DEVICE_TARGET" && -c "$DEVICE_TARGET" ]] || \
    die "Input device is not a character device: $DEVICE_PATH"

EVENT_NAME="$(basename -- "$DEVICE_TARGET")"
DEVICE_NAME="unknown"
if [[ -r "/sys/class/input/$EVENT_NAME/device/name" ]]; then
    DEVICE_NAME="$(cat "/sys/class/input/$EVENT_NAME/device/name")"
fi

BACKLIGHT_ENABLED=0
if [[ "$BACKLIGHT_MODE" == "auto" ]] && command -v brightnessctl >/dev/null 2>&1; then
    if brightnessctl -d "$BACKLIGHT_DEVICE" info >/dev/null 2>&1; then
        BACKLIGHT_ENABLED=1
    fi
fi

log "Detected input device:"
printf '  stable path: %s\n' "$DEVICE_PATH"
printf '  event node:  %s\n' "$DEVICE_TARGET"
printf '  device name: %s\n' "$DEVICE_NAME"

if (( BACKLIGHT_ENABLED )); then
    printf '  resume backlight: %s at %s\n' "$BACKLIGHT_DEVICE" "$BACKLIGHT_LEVEL"
else
    printf '  resume backlight: disabled\n'
fi

confirm "Continue with these settings?" || die "Installation cancelled."

log "Building PalmReject..."
make -C "$ROOT_DIR" clean all

if (( DRY_RUN )); then
    cat <<EOF_PLAN

Dry run complete. No system files were changed.

The installer would create or replace:
  /usr/local/bin/palmreject
  /etc/palmreject.conf
  /etc/systemd/system/palmreject.service
  /usr/lib/systemd/system-sleep/palmreject
  /etc/modules-load.d/palmreject.conf

Detected device:
  $DEVICE_PATH
EOF_PLAN
    exit 0
fi

if (( EUID != 0 )); then
    sudo_args=(
        "$ROOT_DIR/install.sh"
        --device "$DEVICE_PATH"
        --yes
        --backlight-device "$BACKLIGHT_DEVICE"
        --backlight-level "$BACKLIGHT_LEVEL"
        --resume-delay "$RESUME_DELAY"
    )

    if [[ "$BACKLIGHT_MODE" == "off" ]]; then
        sudo_args+=(--no-backlight)
    fi

    if (( FORCE_UNSUPPORTED )); then
        sudo_args+=(--force)
    fi

    exec sudo -- "${sudo_args[@]}"
fi

if [[ -e /etc/systemd/system/palmreject.service || \
      -e /usr/lib/systemd/system-sleep/palmreject || \
      -e /etc/palmreject.conf ]]; then
    warn "An existing PalmReject installation will be replaced."
fi

log "Loading uinput..."
if command -v modprobe >/dev/null 2>&1; then
    modprobe uinput || warn "Could not load uinput immediately."
fi
printf 'uinput\n' > /etc/modules-load.d/palmreject.conf

log "Installing binary..."
make -C "$ROOT_DIR" install PREFIX=/usr/local

log "Writing configuration..."
cat > /etc/palmreject.conf <<EOF_CONFIG
PALMREJECT_DEVICE=$DEVICE_PATH
PALMREJECT_RESUME_DELAY=$RESUME_DELAY
PALMREJECT_RESTORE_BACKLIGHT=$BACKLIGHT_ENABLED
PALMREJECT_BACKLIGHT_DEVICE=$BACKLIGHT_DEVICE
PALMREJECT_BACKLIGHT_LEVEL=$BACKLIGHT_LEVEL
EOF_CONFIG
chmod 0644 /etc/palmreject.conf

log "Installing systemd integration..."
install -Dm644 "$SERVICE_SOURCE" /etc/systemd/system/palmreject.service
install -Dm755 "$SLEEP_SOURCE" /usr/lib/systemd/system-sleep/palmreject

systemctl daemon-reload
systemctl enable palmreject.service
systemctl restart palmreject.service

if systemctl is-active --quiet palmreject.service; then
    log "Installation complete. palmreject.service is active."
else
    warn "The service did not remain active. Recent logs:"
    journalctl -u palmreject.service -n 30 --no-pager || true
    exit 1
fi

cat <<'EOF_DONE'

Useful commands:
  systemctl status palmreject.service
  sudo systemctl restart palmreject.service
  sudo systemctl stop palmreject.service
  journalctl -u palmreject.service -b

Emergency recovery:
  sudo systemctl stop palmreject.service
EOF_DONE
