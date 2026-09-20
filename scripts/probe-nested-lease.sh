#!/usr/bin/env bash
# Diagnose whether nested Hyprland is a viable agent-workspace isolation backend.
# Reads the human compositor, drives only the nested Wayland socket, and records
# each boundary separately so an Aquamarine frame stall is not misdiagnosed as
# an agent-input failure.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
REPORT_ROOT="${HYPR_AGENT_PROBE_DIR:-/tmp/hypr-agent-nested-probe}"
REPORT="$REPORT_ROOT/$(date +%Y%m%d-%H%M%S)-$$"
mkdir -p "$REPORT"

for cmd in Hyprland hyprctl jq grim timeout; do
  command -v "$cmd" >/dev/null || {
    echo "error: required command missing: $cmd" >&2
    exit 2
  }
done

LIVE_SIG="${HYPRLAND_INSTANCE_SIGNATURE:-}"
if [[ -z "$LIVE_SIG" ]]; then
  echo "error: HYPRLAND_INSTANCE_SIGNATURE is empty; run this inside the human Hyprland session" >&2
  exit 2
fi

export HYPR_AGENT_LIVE_SIGNATURE="$LIVE_SIG"

nest_pid=""
app_pid=""
nested_sig=""
nested_socket=""
cleaned=0

hostctl() {
  HYPRLAND_INSTANCE_SIGNATURE="$LIVE_SIG" hyprctl "$@"
}

nestedctl() {
  hyprctl -i "$nested_sig" "$@"
}

capture_host_state() {
  local prefix=$1
  hostctl -j activeworkspace | jq '{id, name, monitor}' >"$REPORT/$prefix-workspace.json"
  hostctl -j activewindow | jq '{
    address,
    class,
    workspace: (.workspace.id // null),
    monitor
  }' >"$REPORT/$prefix-window.json"
  hostctl -j cursorpos | jq '{x, y}' >"$REPORT/$prefix-cursor.json"
}

same_host_state() {
  local left=$1 right=$2
  cmp -s "$REPORT/$left-workspace.json" "$REPORT/$right-workspace.json" &&
    cmp -s "$REPORT/$left-window.json" "$REPORT/$right-window.json" &&
    cmp -s "$REPORT/$left-cursor.json" "$REPORT/$right-cursor.json"
}

result() {
  printf '%s=%s\n' "$1" "$2" | tee -a "$REPORT/results.env" >/dev/null
}

stop_pid() {
  local pid=$1
  [[ -n "$pid" ]] || return 0
  kill -0 "$pid" 2>/dev/null || return 0
  kill -TERM "$pid" 2>/dev/null || true
  for _ in {1..20}; do
    kill -0 "$pid" 2>/dev/null || return 0
    sleep 0.1
  done
  kill -KILL "$pid" 2>/dev/null || true
}

cleanup() {
  (( cleaned )) && return 0
  cleaned=1
  set +e
  stop_pid "$app_pid"
  stop_pid "$nest_pid"
}
trap cleanup EXIT INT TERM

printf 'report_dir=%s\n' "$REPORT"
{
  echo "date=$(date --iso-8601=seconds)"
  echo "kernel=$(uname -srmo)"
  echo "live_his=$LIVE_SIG"
  echo "host_wayland=${WAYLAND_DISPLAY:-}"
  echo
  echo "== hyprctl version =="
  hostctl version
  echo
  echo "== packages =="
  pacman -Q hyprland aquamarine 2>/dev/null || true
} >"$REPORT/system.txt"

hostctl -j instances >"$REPORT/instances-before.json"
jq -r '.[].instance' "$REPORT/instances-before.json" >"$REPORT/instances-before.txt"
capture_host_state host-before

echo "Starting nested Hyprland. No plugin will be loaded."
"$ROOT/scripts/start-nested-hyprland.sh" >"$REPORT/nested.log" 2>&1 &
nest_pid=$!

deadline=$((SECONDS + 20))
while (( SECONDS < deadline )); do
  hyprctl -j instances >"$REPORT/instances-current.json" 2>/dev/null || true
  mapfile -t new_instances < <(
    jq -r '.[].instance' "$REPORT/instances-current.json" 2>/dev/null |
      while read -r sig; do
        grep -Fxq "$sig" "$REPORT/instances-before.txt" || printf '%s\n' "$sig"
      done
  )
  if (( ${#new_instances[@]} == 1 )); then
    nested_sig=${new_instances[0]}
    break
  elif (( ${#new_instances[@]} > 1 )); then
    echo "error: more than one new Hyprland instance appeared; refusing ambiguous target" >&2
    result nested_instance AMBIGUOUS
    exit 1
  fi
  kill -0 "$nest_pid" 2>/dev/null || {
    echo "error: nested Hyprland exited before registering an instance" >&2
    result nested_instance START_FAILED
    exit 1
  }
  sleep 0.25
done

if [[ -z "$nested_sig" ]]; then
  echo "error: no new Hyprland instance appeared within 20s" >&2
  result nested_instance TIMEOUT
  exit 1
fi

if [[ "$nested_sig" == "$LIVE_SIG" ]]; then
  echo "error: nested HIS equals live HIS; refusing" >&2
  result nested_instance LIVE_COLLISION
  exit 1
fi

nested_socket=$(hyprctl -j instances |
  jq -r --arg sig "$nested_sig" '.[] | select(.instance == $sig) | .wl_socket' |
  head -n1)

if [[ -z "$nested_socket" || "$nested_socket" == "null" ]]; then
  echo "error: nested instance has no Wayland socket" >&2
  result nested_instance NO_WAYLAND_SOCKET
  exit 1
fi

printf '%s\n' "$nested_sig" >"$REPORT/nested-his.txt"
printf '%s\n' "$nested_socket" >"$REPORT/nested-wayland.txt"
result nested_instance PASS

deadline=$((SECONDS + 10))
until nestedctl -j monitors >"$REPORT/nested-monitors.json" 2>/dev/null; do
  (( SECONDS < deadline )) || {
    echo "error: nested hyprctl never became responsive" >&2
    result nested_hyprctl TIMEOUT
    exit 1
  }
  sleep 0.2
done
result nested_hyprctl PASS

capture_host_state host-after-start
if same_host_state host-before host-after-start; then
  result startup_host_state PASS
else
  result startup_host_state FAIL
fi

# First distinguish compositor presentation from lease/input behavior. Aquamarine
# #348 reports a nested Wayland backend that presents once and then leaves every
# screencopy request blocked forever. Bound the call so this probe classifies
# that failure instead of hanging with it.
if timeout 6 env \
    WAYLAND_DISPLAY="$nested_socket" \
    HYPRLAND_INSTANCE_SIGNATURE="$nested_sig" \
    grim "$REPORT/nested-before.png" >/dev/null 2>"$REPORT/grim-before.err"; then
  result nested_screencopy_before PASS
else
  result nested_screencopy_before FAIL
  cat <<EOF
Nested screencopy did not complete. This may match Aquamarine #348:
https://github.com/hyprwm/aquamarine/issues/348

The report is still useful. Do not interpret this as a workspace-lease or input failure.
EOF
fi

probe_class="HyprAgentProbe-$$"
input_file="$REPORT/input.txt"
sentinel="nested-input-$$"

launch_probe_app() {
  if command -v kitty >/dev/null; then
    env \
      WAYLAND_DISPLAY="$nested_socket" \
      HYPRLAND_INSTANCE_SIGNATURE="$nested_sig" \
      PROBE_INPUT_FILE="$input_file" \
      kitty --class "$probe_class" --title "$probe_class" \
        bash -lc 'IFS= read -r line; printf "%s\n" "$line" >"$PROBE_INPUT_FILE"; exec sleep infinity' \
        >"$REPORT/app.log" 2>&1 &
  elif command -v foot >/dev/null; then
    env \
      WAYLAND_DISPLAY="$nested_socket" \
      HYPRLAND_INSTANCE_SIGNATURE="$nested_sig" \
      PROBE_INPUT_FILE="$input_file" \
      foot --app-id "$probe_class" --title "$probe_class" \
        bash -lc 'IFS= read -r line; printf "%s\n" "$line" >"$PROBE_INPUT_FILE"; exec sleep infinity' \
        >"$REPORT/app.log" 2>&1 &
  else
    return 1
  fi
  app_pid=$!
}

if ! launch_probe_app; then
  result nested_app SKIP_NO_TERMINAL
else
  deadline=$((SECONDS + 10))
  while (( SECONDS < deadline )); do
    if nestedctl -j clients |
      jq -e --arg class "$probe_class" '.[] | select(.class == $class)' >/dev/null 2>&1; then
      result nested_app PASS
      break
    fi
    sleep 0.2
  done
  if ! grep -q '^nested_app=PASS$' "$REPORT/results.env"; then
    result nested_app FAIL
  fi
fi

if grep -q '^nested_app=PASS$' "$REPORT/results.env"; then
  nestedctl dispatch focuswindow "class:^$probe_class$" >/dev/null 2>&1 || true
  nestedctl -j cursorpos | jq '{x, y}' >"$REPORT/nested-cursor-before.json"
  capture_host_state host-before-input

  if command -v wdotool >/dev/null; then
    result input_backend wdotool-wlr-protocols

    if timeout 6 env \
      WAYLAND_DISPLAY="$nested_socket" \
      HYPRLAND_INSTANCE_SIGNATURE="$nested_sig" \
      XDG_CURRENT_DESKTOP=Hyprland \
      wdotool --backend wlr-protocols mousemove 123 77 >/dev/null 2>"$REPORT/wdotool-pointer.err"; then
      nestedctl -j cursorpos | jq '{x, y}' >"$REPORT/nested-cursor-after.json"
      if cmp -s "$REPORT/nested-cursor-before.json" "$REPORT/nested-cursor-after.json"; then
        result nested_pointer FAIL_NO_MOVEMENT
      else
        result nested_pointer PASS
      fi
    else
      result nested_pointer FAIL
    fi

    if timeout 6 env \
      WAYLAND_DISPLAY="$nested_socket" \
      HYPRLAND_INSTANCE_SIGNATURE="$nested_sig" \
      XDG_CURRENT_DESKTOP=Hyprland \
      wdotool --backend wlr-protocols type "$sentinel" >/dev/null 2>"$REPORT/wdotool-keyboard.err" &&
      timeout 6 env \
        WAYLAND_DISPLAY="$nested_socket" \
        HYPRLAND_INSTANCE_SIGNATURE="$nested_sig" \
        XDG_CURRENT_DESKTOP=Hyprland \
        wdotool --backend wlr-protocols key Return >/dev/null 2>>"$REPORT/wdotool-keyboard.err"; then
      deadline=$((SECONDS + 5))
      while (( SECONDS < deadline )); do
        [[ -f "$input_file" ]] && break
        sleep 0.1
      done
      if [[ -f "$input_file" && $(<"$input_file") == "$sentinel" ]]; then
        result nested_keyboard PASS
      else
        result nested_keyboard FAIL_NOT_DELIVERED
      fi
    else
      result nested_keyboard FAIL
    fi

    capture_host_state host-after-input
    if same_host_state host-before-input host-after-input; then
      result input_host_state PASS
    else
      result input_host_state FAIL
    fi
  else
    result input_backend SKIP_NO_WDOTOOL
    result nested_pointer SKIP
    result nested_keyboard SKIP
    result input_host_state SKIP
    cat <<'EOF'
Full nested input proof needs wdotool's wlr-protocols backend so input is sent
to the nested Wayland socket instead of /dev/uinput. On Arch/CachyOS it is
available from the AUR as wdotool or wdotool-bin.
EOF
  fi

  if timeout 6 env \
      WAYLAND_DISPLAY="$nested_socket" \
      HYPRLAND_INSTANCE_SIGNATURE="$nested_sig" \
      grim "$REPORT/nested-after.png" >/dev/null 2>"$REPORT/grim-after.err"; then
    result nested_screencopy_after PASS
  else
    result nested_screencopy_after FAIL
  fi
fi

stop_pid "$app_pid"
app_pid=""
stop_pid "$nest_pid"
nest_pid=""

deadline=$((SECONDS + 5))
while (( SECONDS < deadline )); do
  if ! hyprctl -j instances |
      jq -e --arg sig "$nested_sig" '.[] | select(.instance == $sig)' >/dev/null 2>&1; then
    result cleanup PASS
    break
  fi
  sleep 0.2
done
if ! grep -q '^cleanup=PASS$' "$REPORT/results.env"; then
  result cleanup FAIL
fi

capture_host_state host-final

{
  echo
  echo "== results =="
  cat "$REPORT/results.env"
  echo
  echo "== report directory =="
  echo "$REPORT"
  echo
  echo "Attach results.env, system.txt, nested.log, and the host/nested JSON files to:"
  echo "https://github.com/kvnloo/hypr-agent-cursors/issues/6"
} | tee "$REPORT/summary.txt"

cleaned=1
trap - EXIT INT TERM
