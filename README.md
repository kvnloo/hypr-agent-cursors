# hypr-agent-cursors

Work-in-progress Hyprland plugin *contract* for agent cursors/keyboards that do not share the human `wl_seat`. Unit + ABI only until nested load is proven.

Hyprland exposes one seat by design (hyprwm/Hyprland#10336). This repo keeps agent
cursor, keyboard focus, and pressed-state in an out-of-tree registry. The human
`seat0` is never replaced.

## Status (honest)

| Layer | State |
|-------|--------|
| Unit: multi-agent cursor isolation | GREEN (`make test-seats`) |
| Unit: multi-agent keyboard isolation | GREEN (press/release/host split) |
| Unit: virtual-monitor bind + clamp + live denylist | GREEN (`make test-vout`) |
| Unit: N-agent parallel workflow simulator | GREEN (`make test-parallel`, 4-agent scenario) |
| Plugin ABI shim (`pluginAPIVersion`/`pluginInit`/`pluginExit`) | GREEN (`make test-abi`) |
| Full `hypr_agent_cursors.so` link | builds; offline dlopen fails (needs compositor symbols) ([#3](https://github.com/kvnloo/hypr-agent-cursors/issues/3), [#4](https://github.com/kvnloo/hypr-agent-cursors/issues/4)) |
| Nested Hyprland + visible multi-cursor + real KB inject | **NOT PROVEN** ([#1](https://github.com/kvnloo/hypr-agent-cursors/issues/1), [#2](https://github.com/kvnloo/hypr-agent-cursors/issues/2)) |
| Live seat mutations | **FORBIDDEN** — never done in this work ([#5](https://github.com/kvnloo/hypr-agent-cursors/issues/5)) |

A virtual headless output on the **live** instance still shares `seat0`. That is not
multi-cursor. Nested HIS only.

## Prove unit layer

```bash
make test
```

## Plugin

```bash
make shim   # offline ABI .so — safe to dlopen in tests
make full   # compositor plugin skeleton — load ONLY in nested Hyprland
```

Load (nested only):

```bash
export HYPR_AGENT_LIVE_SIGNATURE="$HYPRLAND_INSTANCE_SIGNATURE"  # capture human HIS first
# start nest (opens a window): ./scripts/start-nested-hyprland.sh
# then, with nest HIS != live:
./scripts/never-touch-live.sh hyprctl plugin load /abs/path/build/hypr_agent_cursors.so
```

`scripts/never-touch-live.sh` refuses when target HIS equals live.

## Parallel TDD evidence

Four subagent workdirs under `/tmp/swarm-hypr-agent/worker-{kb,vout,abi,parallel}/`.
Merged RESULT copies: `.audit/worker-results/`.

## Open work

| Issue | What |
|-------|------|
| [#1](https://github.com/kvnloo/hypr-agent-cursors/issues/1) | Nested HIS load + visible second cursor |
| [#2](https://github.com/kvnloo/hypr-agent-cursors/issues/2) | Agent keyboard inject without touching host seat |
| [#3](https://github.com/kvnloo/hypr-agent-cursors/issues/3) | Wire `plugin/main.cpp` to HyprlandAPI (nest-only) |
| [#4](https://github.com/kvnloo/hypr-agent-cursors/issues/4) | Offline full-`.so` `dlopen(RTLD_NOW)` fails by design |
| [#5](https://github.com/kvnloo/hypr-agent-cursors/issues/5) | Disclaimers: not a second `wl_seat`, never touch live HIS |

Do not start the nest or `hyprctl plugin load` against live. `scripts/never-touch-live.sh` refuses when target HIS equals `HYPR_AGENT_LIVE_SIGNATURE`.

## Not claimed

- Second `wl_seat` in Hyprland core
- Visible second cursor on your eDP-1 session
- Multi-cursor via live `output create headless` / wayvnc (still one seat)
- Working computer-use inject that does not move `CPointerManager::m_pointerPos`
- Local `media/` mockups — not in git, not the plugin contract
