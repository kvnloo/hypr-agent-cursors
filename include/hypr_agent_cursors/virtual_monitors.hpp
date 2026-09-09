#pragma once

// Pure domain model for virtual-monitor binding and multi-agent cursors.
// No compositor / hyprctl / Wayland — unit-test only.
//
// move_agent policy: CLAMP cursor to inclusive pixel range
//   [0, width - 1] x [0, height - 1]
// of the agent's bound VirtualMonitor. Out-of-range coords are not errors.

#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace hypr_agent_cursors {

struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

struct VirtualMonitor {
    std::string name;
    int width = 0;
    int height = 0;
    bool is_headless = false;
};

class VirtualMonitorRegistry {
  public:
    void register_monitor(VirtualMonitor m);
    const VirtualMonitor& get(const std::string& name) const;
    bool contains(const std::string& name) const;

  private:
    std::unordered_map<std::string, VirtualMonitor> monitors_;
};

// Rejects binding agents to denylisted live output names (e.g. eDP-1).
class LiveOutputPolicy {
  public:
    void deny(std::string name);
    bool allows_bind(const std::string& name) const;

  private:
    std::unordered_set<std::string> denylist_;
};

struct AgentCursor {
    std::string name;
    std::string bound_output;
    Vec2 cursor;
    bool enabled = true;
};

class AgentCursorRegistry {
  public:
    explicit AgentCursorRegistry(const VirtualMonitorRegistry& monitors);
    AgentCursorRegistry(const VirtualMonitorRegistry& monitors, LiveOutputPolicy policy);

    AgentCursor& create(const std::string& name, const std::string& bound_output);
    AgentCursor& agent(const std::string& name);
    const AgentCursor& agent(const std::string& name) const;

    void move_agent(const std::string& name, Vec2 pos);

  private:
    const VirtualMonitorRegistry& monitors_;
    LiveOutputPolicy policy_;
    std::unordered_map<std::string, AgentCursor> agents_;
};

} // namespace hypr_agent_cursors
