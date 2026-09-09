#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace hypr_agent_cursors {

struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

struct AgentSeat {
    std::string name;
    std::string bound_output;
    Vec2 cursor;
    std::optional<std::string> keyboard_focus;
    std::unordered_set<std::uint32_t> pressed_buttons;
    std::unordered_set<std::uint32_t> pressed_keys;
    bool enabled = true;
};

struct HostSeat {
    std::string name = "seat0";
    Vec2 cursor;
    std::optional<std::string> keyboard_focus;
    std::unordered_set<std::uint32_t> pressed_keys;
};

class AgentSeatRegistry {
  public:
    HostSeat& host();
    const HostSeat& host() const;

    AgentSeat& create(const std::string& name, const std::string& bound_output);
    AgentSeat& agent(const std::string& name);
    const AgentSeat& agent(const std::string& name) const;

    void move_agent(const std::string& name, Vec2 pos);
    void move_host(Vec2 pos);
    void focus_agent_keyboard(const std::string& name, std::string surface);
    void press_key(const std::string& name, std::uint32_t keycode);
    void release_key(const std::string& name, std::uint32_t keycode);
    void host_press_key(std::uint32_t keycode);
    void host_release_key(std::uint32_t keycode);
    void disable(const std::string& name);

    bool last_host_cursor_moved() const;

  private:
    HostSeat host_;
    std::unordered_map<std::string, AgentSeat> agents_;
    bool last_host_cursor_moved_ = false;
};

} // namespace hypr_agent_cursors
