#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace hypr_agent_cursors {

struct Vec2 {
    double x = 0.0;
    double y = 0.0;

    bool operator==(const Vec2& o) const { return x == o.x && y == o.y; }
};

struct HostSeat {
    std::string name = "seat0";
    Vec2 cursor;
    std::optional<std::string> keyboard_focus;
};

struct AgentSeat {
    std::string name;
    std::string bound_output;
    Vec2 cursor;
    std::optional<std::string> keyboard_focus;
    std::unordered_set<std::uint32_t> pressed_buttons;
    std::unordered_set<std::uint32_t> pressed_keys;
    bool enabled = true;
    bool alive = true; // false after kill(); distinct from enabled for bulk kill-switch semantics
};

struct HostSnapshot {
    Vec2 cursor;
    std::optional<std::string> keyboard_focus;
    std::string name;
};

// How agents bind to virtual outputs without a compositor.
enum class OutputBindingMode {
    PerAgent,   // agent-i -> HEADLESS-i
    SharedPool, // all agents -> HEADLESS-POOL
};

// One atomic agent (or host) action for deterministic interleaving.
enum class SessionOp {
    Move,
    PressKey,
    ReleaseKey,
    Focus,
    HostMove,
    HostFocus,
};

struct SessionStep {
    SessionOp op = SessionOp::Move;
    std::string agent; // empty for host ops
    Vec2 pos{};
    std::uint32_t keycode = 0;
    std::string surface;
};

// Multi-agent computer-use session simulator.
// Models N independent agent cursor+kb streams that must never mutate host seat0
// unless an explicit host op is scheduled.
class MultiAgentSession {
  public:
    MultiAgentSession() = default;

    const HostSeat& host() const;
    HostSeat& host();

    // Create agents agent-1 .. agent-N with the chosen output binding.
    void create_agents(std::size_t n, OutputBindingMode mode);

    AgentSeat& agent(const std::string& name);
    const AgentSeat& agent(const std::string& name) const;

    std::size_t agent_count() const;
    std::vector<std::string> agent_names() const;

    // Agent ops — never touch host cursor/focus.
    void move(const std::string& name, Vec2 pos);
    void press_key(const std::string& name, std::uint32_t keycode);
    void release_key(const std::string& name, std::uint32_t keycode);
    void focus(const std::string& name, std::string surface);

    // Explicit host ops only.
    void move_host(Vec2 pos);
    void focus_host(std::string surface);

    // Kill one agent; others must keep working.
    void kill(const std::string& name);

    // Bulk kill switch: disable every agent, release keys/buttons, clear focus.
    void bulk_disable_all();

    // Deterministic interleaving of concurrent agent workflows.
    void run_schedule(const std::vector<SessionStep>& steps);

    // Build a round-robin schedule: for each phase, one step per agent in order.
    // Each agent i gets move -> press_key -> focus on distinct surface.
    static std::vector<SessionStep> make_independent_workflows(
        const std::vector<std::string>& names,
        const std::vector<Vec2>& positions,
        const std::vector<std::uint32_t>& keycodes,
        const std::vector<std::string>& surfaces);

    HostSnapshot snapshot_host() const;

    // True iff host cursor/focus/name match the snapshot (invariant helper).
    bool host_unchanged_since(const HostSnapshot& snap) const;

  private:
    void require_alive_enabled(const std::string& name);
    static std::string agent_name_for_index(std::size_t i); // agent-1 ..
    static std::string output_for(std::size_t i, OutputBindingMode mode);

    HostSeat host_;
    std::unordered_map<std::string, AgentSeat> agents_;
    std::vector<std::string> order_; // creation order
};

} // namespace hypr_agent_cursors
