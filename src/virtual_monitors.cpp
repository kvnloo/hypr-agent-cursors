#include "hypr_agent_cursors/virtual_monitors.hpp"

#include <algorithm>
#include <utility>

namespace hypr_agent_cursors {

void VirtualMonitorRegistry::register_monitor(VirtualMonitor m) {
    if (m.name.empty())
        throw std::invalid_argument("monitor name must be non-empty");
    if (m.width <= 0 || m.height <= 0)
        throw std::invalid_argument("monitor size must be positive");
    if (monitors_.contains(m.name))
        throw std::invalid_argument("duplicate monitor name");
    monitors_.emplace(m.name, std::move(m));
}

const VirtualMonitor& VirtualMonitorRegistry::get(const std::string& name) const {
    auto it = monitors_.find(name);
    if (it == monitors_.end())
        throw std::out_of_range("unknown monitor");
    return it->second;
}

bool VirtualMonitorRegistry::contains(const std::string& name) const {
    return monitors_.contains(name);
}

void LiveOutputPolicy::deny(std::string name) {
    if (name.empty())
        throw std::invalid_argument("deny name must be non-empty");
    denylist_.insert(std::move(name));
}

bool LiveOutputPolicy::allows_bind(const std::string& name) const {
    return !denylist_.contains(name);
}

AgentCursorRegistry::AgentCursorRegistry(const VirtualMonitorRegistry& monitors)
    : monitors_(monitors), policy_() {}

AgentCursorRegistry::AgentCursorRegistry(const VirtualMonitorRegistry& monitors,
                                         LiveOutputPolicy policy)
    : monitors_(monitors), policy_(std::move(policy)) {}

AgentCursor& AgentCursorRegistry::create(const std::string& name,
                                         const std::string& bound_output) {
    if (name.empty())
        throw std::invalid_argument("agent name must be non-empty");
    if (agents_.contains(name))
        throw std::invalid_argument("duplicate agent name");
    if (!monitors_.contains(bound_output))
        throw std::invalid_argument("bound_output must reference a known monitor");
    if (!policy_.allows_bind(bound_output))
        throw std::invalid_argument("live output bind forbidden by policy");

    AgentCursor seat;
    seat.name         = name;
    seat.bound_output = bound_output;
    seat.enabled      = true;
    seat.cursor       = {0.0, 0.0};
    auto [it, inserted] = agents_.emplace(name, std::move(seat));
    (void)inserted;
    return it->second;
}

AgentCursor& AgentCursorRegistry::agent(const std::string& name) {
    auto it = agents_.find(name);
    if (it == agents_.end())
        throw std::out_of_range("unknown agent");
    return it->second;
}

const AgentCursor& AgentCursorRegistry::agent(const std::string& name) const {
    auto it = agents_.find(name);
    if (it == agents_.end())
        throw std::out_of_range("unknown agent");
    return it->second;
}

void AgentCursorRegistry::move_agent(const std::string& name, Vec2 pos) {
    auto& a = agent(name);
    if (!a.enabled)
        throw std::invalid_argument("agent disabled");

    const VirtualMonitor& mon = monitors_.get(a.bound_output);
    const double max_x = static_cast<double>(mon.width - 1);
    const double max_y = static_cast<double>(mon.height - 1);

    a.cursor.x = std::clamp(pos.x, 0.0, max_x);
    a.cursor.y = std::clamp(pos.y, 0.0, max_y);
}

} // namespace hypr_agent_cursors
