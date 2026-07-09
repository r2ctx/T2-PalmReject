#!/usr/bin/env bash
set -Eeuo pipefail

KEEP_CONFIG=0

usage() {
    cat <<'USAGE'
Usage: ./uninstall.sh [--keep-config]

Remove the PalmReject service, suspend/resume hook, installed binary, and
uinput module-load configuration.

Options:
  --keep-config   Preserve /etc/palmreject.conf.
  -h, --help      Show this help.
USAGE
}

while (( $# > 0 )); do
    case "$1" in
        --keep-config)
            KEEP_CONFIG=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            usage >&2
            echo "Unknown option: $1" >&2
            exit 2
            ;;
    esac
done

if (( EUID != 0 )); then
    args=("$(readlink -f -- "$0")")
    (( KEEP_CONFIG )) && args+=(--keep-config)
    exec sudo -- "${args[@]}"
fi

systemctl disable --now palmreject.service >/dev/null 2>&1 || true

rm -f /etc/systemd/system/palmreject.service
rm -f /usr/lib/systemd/system-sleep/palmreject
rm -f /usr/local/bin/palmreject
rm -f /etc/modules-load.d/palmreject.conf
rm -f /run/palmreject-was-active

if (( ! KEEP_CONFIG )); then
    rm -f /etc/palmreject.conf
fi

systemctl daemon-reload
systemctl reset-failed palmreject.service >/dev/null 2>&1 || true

echo "PalmReject was removed."
if (( KEEP_CONFIG )); then
    echo "Kept /etc/palmreject.conf."
fi
