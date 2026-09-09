#include "hypr_agent_cursors/agent_seats.hpp"

namespace hypr_agent_cursors {

HostSeat& AgentSeatRegistry::host() {
    return host_;
}

const HostSeat& AgentSeatRegistry::host() const {
    return host_;
}

AgentSeat& AgentSeatRegistry::create(const std::string& name, const std::string& bound_output) {
    if (name.empty() || name == host_.name)
        throw std::invalid_argument("agent name must be non-empty and not seat0");
    if (agents_.contains(name))
        throw std::invalid_argument("duplicate agent name");
    AgentSeat seat;
    seat.name          = name;
    seat.bound_output  = bound_output;
    seat.enabled       = true;
    auto [it, inserted] = agents_.emplace(name, std::move(seat));
    (void)inserted;
    last_host_cursor_moved_ = false;
    return it->second;
}

AgentSeat& AgentSeatRegistry::agent(const std::string& name) {
    auto it = agents_.find(name);
    if (it == agents_.end())
        throw std::out_of_range("unknown agent");
    return it->second;
}

const AgentSeat& AgentSeatRegistry::agent(const std::string& name) const {
    auto it = agents_.find(name);
    if (it == agents_.end())
        throw std::out_of_range("unknown agent");
    return it->second;
}

void AgentSeatRegistry::move_agent(const std::string& name, Vec2 pos) {
    auto& a = agent(name);
    if (!a.enabled)
        throw std::invalid_argument("agent disabled");
    a.cursor                = pos;
    last_host_cursor_moved_ = false;
}

void AgentSeatRegistry::move_host(Vec2 pos) {
    host_.cursor            = pos;
    last_host_cursor_moved_ = true;
}

void AgentSeatRegistry::focus_agent_keyboard(const std::string& name, std::string surface) {
    auto& a = agent(name);
    if (!a.enabled)
        throw std::invalid_argument("agent disabled");
    a.keyboard_focus = std::move(surface);
}

void AgentSeatRegistry::press_key(const std::string& name, std::uint32_t keycode) {
    auto& a = agent(name);
    if (!a.enabled)
        throw std::invalid_argument("agent disabled");
    a.pressed_keys.insert(keycode);
}

void AgentSeatRegistry::release_key(const std::string& name, std::uint32_t keycode) {
    auto& a = agent(name);
    if (!a.enabled)
        throw std::invalid_argument("agent disabled");
    a.pressed_keys.erase(keycode);
}

void AgentSeatRegistry::host_press_key(std::uint32_t keycode) {
    host_.pressed_keys.insert(keycode);
}

void AgentSeatRegistry::host_release_key(std::uint32_t keycode) {
    host_.pressed_keys.erase(keycode);
}

void AgentSeatRegistry::disable(const std::string& name) {
    auto& a = agent(name);
    a.enabled = false;
    a.pressed_buttons.clear();
    a.pressed_keys.clear();
    a.keyboard_focus.reset();
}

bool AgentSeatRegistry::last_host_cursor_moved() const {
    return last_host_cursor_moved_;
}

} // namespace hypr_agent_cursors
