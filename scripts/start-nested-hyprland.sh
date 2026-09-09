#!/usr/bin/env bash
# Start a nested Hyprland for plugin development. Never targets the live human seat.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

LIVE_SIG="${HYPR_AGENT_LIVE_SIGNATURE:-${HYPRLAND_INSTANCE_SIGNATURE:-}}"
if [[ -z "${LIVE_SIG}" ]]; then
  echo "error: cannot determine live HYPRLAND_INSTANCE_SIGNATURE" >&2
  exit 2
fi

RUNTIME="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/hypr-agent-cursors-nest"
mkdir -p "$RUNTIME" "$ROOT/nest"
CONF="$ROOT/nest/hyprlandd.lua"
if [[ ! -f "$CONF" ]]; then
  cat >"$CONF" <<'LUA'
-- Minimal nested config for hypr-agent-cursors. Alt binds only.
-- No exec-once that touches the host session.
general = {
  border_size = 2,
}
misc = {
  disable_hyprland_logo = true,
  force_default_wallpaper = 0,
  vfr = true,
}
debug = {
  watchdog_timeout = 0,
}
-- Headless agent canvas name used by unit tests / plugin dispatchers later.
-- Created only inside THIS nest via hyprctl after HIS differs from live.
LUA
fi

export HYPR_AGENT_LIVE_SIGNATURE="$LIVE_SIG"
# Nested Hyprland inherits WAYLAND_DISPLAY and opens as a client window.
# Do not unset WAYLAND_DISPLAY — nest must be a child of the human compositor.
unset HYPRLAND_INSTANCE_SIGNATURE
export HYPRLAND_CONFIG="$CONF"

echo "live_sig=$LIVE_SIG"
echo "config=$CONF"
echo "runtime=$RUNTIME"
echo "Starting nested Hyprland (window). Load plugins only after nest HIS != live."
echo "Refusing any hyprctl whose target HIS equals live."

# Record baseline host monitors for post-check (parent must verify).
if command -v hyprctl >/dev/null 2>&1 && [[ -n "${HYPRLAND_INSTANCE_SIGNATURE:-}" || -n "$LIVE_SIG" ]]; then
  # Query live explicitly via env
  HYPRLAND_INSTANCE_SIGNATURE="$LIVE_SIG" hyprctl -j monitors 2>/dev/null \
    | python3 -c 'import json,sys; print("live_monitors", [m["name"] for m in json.load(sys.stdin)])' \
    || true
fi

exec Hyprland --config "$CONF"
