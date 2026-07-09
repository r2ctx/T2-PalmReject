#!/usr/bin/env bash
set -euo pipefail

print_name() {
    local link="$1"
    local target event sys_name

    target="$(readlink -f -- "$link" 2>/dev/null || true)"
    [[ -n "$target" ]] || return 1

    event="$(basename -- "$target")"
    sys_name="/sys/class/input/${event}/device/name"
    [[ -r "$sys_name" ]] || return 1

    cat "$sys_name"
}

is_supported_name() {
    local name="$1"

    [[ "$name" == *"Apple Internal Keyboard / Trackpad"* ]] ||
    [[ "$name" == *"Apple Internal Keyboard"*"Trackpad"* ]]
}

collect_candidates() {
    local link name target
    local -A seen_targets=()
    local -a links=()

    shopt -s nullglob

    # Prefer by-id because it follows the physical device identity.
    links+=(/dev/input/by-id/*-event-mouse)
    links+=(/dev/input/by-path/*-event-mouse)

    for link in "${links[@]}"; do
        [[ -L "$link" || -e "$link" ]] || continue

        target="$(readlink -f -- "$link" 2>/dev/null || true)"
        [[ -c "$target" ]] || continue
        [[ -z "${seen_targets[$target]+x}" ]] || continue

        name="$(print_name "$link" 2>/dev/null || true)"
        is_supported_name "$name" || continue

        seen_targets["$target"]=1
        printf '%s\t%s\t%s\n' "$link" "$target" "$name"
    done
}

usage() {
    cat <<'USAGE'
Usage: detect-trackpad.sh [--all]

Without options, prints the preferred stable path for the first supported
Apple internal keyboard/trackpad device.

Options:
  --all   Print all matching candidates with their event node and name.
  -h, --help
USAGE
}

mode="first"
case "${1:-}" in
    "") ;;
    --all) mode="all" ;;
    -h|--help) usage; exit 0 ;;
    *) usage >&2; exit 2 ;;
esac

mapfile -t candidates < <(collect_candidates)

if (( ${#candidates[@]} == 0 )); then
    echo "No supported Apple internal trackpad was detected." >&2
    exit 1
fi

if [[ "$mode" == "all" ]]; then
    printf '%s\n' "${candidates[@]}"
else
    IFS=$'\t' read -r stable_path _event_path _name <<< "${candidates[0]}"
    printf '%s\n' "$stable_path"
fi
