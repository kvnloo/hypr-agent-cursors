#!/usr/bin/env bash
# Refuse compositor mutations against the live human Hyprland instance.
set -euo pipefail
LIVE_SIG="${HYPR_AGENT_LIVE_SIGNATURE:-}"
TARGET_SIG="${HYPRLAND_INSTANCE_SIGNATURE:-}"
if [[ -z "$LIVE_SIG" ]]; then
  echo "set HYPR_AGENT_LIVE_SIGNATURE to the human session signature" >&2
  exit 2
fi
if [[ -z "$TARGET_SIG" ]]; then
  echo "HYPRLAND_INSTANCE_SIGNATURE empty; refusing" >&2
  exit 1
fi
if [[ "$TARGET_SIG" == "$LIVE_SIG" ]]; then
  echo "refusing to touch the live human seat ($TARGET_SIG)" >&2
  exit 1
fi
exec "$@"
