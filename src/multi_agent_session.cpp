#include "hypr_agent_cursors/multi_agent_session.hpp"

#include <stdexcept>

namespace hypr_agent_cursors {

const HostSeat& MultiAgentSession::host() const {
    return host_;
}

HostSeat& MultiAgentSession::host() {
    return host_;
}

std::string MultiAgentSession::agent_name_for_index(std::size_t i) {
    return "agent-" + std::to_string(i);
}

std::string MultiAgentSession::output_for(std::size_t i, OutputBindingMode mode) {
    if (mode == OutputBindingMode::SharedPool)
        return "HEADLESS-POOL";
    return "HEADLESS-" + std::to_string(i);
}

void MultiAgentSession::create_agents(std::size_t n, OutputBindingMode mode) {
    if (n == 0)
        throw std::invalid_argument("agent count must be > 0");
    if (!agents_.empty())
        throw std::invalid_argument("agents already created for this session");

    host_.name = "seat0";
    agents_.clear();
    order_.clear();
    order_.reserve(n);

    for (std::size_t i = 1; i <= n; ++i) {
        AgentSeat seat;
        seat.name         = agent_name_for_index(i);
        seat.bound_output = output_for(i, mode);
        seat.enabled      = true;
        seat.alive        = true;
        order_.push_back(seat.name);
        agents_.emplace(seat.name, std::move(seat));
    }
}

AgentSeat& MultiAgentSession::agent(const std::string& name) {
    auto it = agents_.find(name);
    if (it == agents_.end())
        throw std::out_of_range("unknown agent");
    return it->second;
}

const AgentSeat& MultiAgentSession::agent(const std::string& name) const {
    auto it = agents_.find(name);
    if (it == agents_.end())
        throw std::out_of_range("unknown agent");
    return it->second;
}

std::size_t MultiAgentSession::agent_count() const {
    return agents_.size();
}

std::vector<std::string> MultiAgentSession::agent_names() const {
    return order_;
}

void MultiAgentSession::require_alive_enabled(const std::string& name) {
    auto& a = agent(name);
    if (!a.alive || !a.enabled)
        throw std::invalid_argument("agent disabled or killed");
}

void MultiAgentSession::move(const std::string& name, Vec2 pos) {
    require_alive_enabled(name);
    agent(name).cursor = pos;
}

void MultiAgentSession::press_key(const std::string& name, std::uint32_t keycode) {
    require_alive_enabled(name);
    agent(name).pressed_keys.insert(keycode);
}

void MultiAgentSession::release_key(const std::string& name, std::uint32_t keycode) {
    require_alive_enabled(name);
    agent(name).pressed_keys.erase(keycode);
}

void MultiAgentSession::focus(const std::string& name, std::string surface) {
    require_alive_enabled(name);
    agent(name).keyboard_focus = std::move(surface);
}

void MultiAgentSession::move_host(Vec2 pos) {
    host_.cursor = pos;
}

void MultiAgentSession::focus_host(std::string surface) {
    host_.keyboard_focus = std::move(surface);
}

void MultiAgentSession::kill(const std::string& name) {
    auto& a = agent(name);
    a.alive   = false;
    a.enabled = false;
    a.pressed_buttons.clear();
    a.pressed_keys.clear();
    a.keyboard_focus.reset();
}

void MultiAgentSession::bulk_disable_all() {
    for (auto& [_, a] : agents_) {
        a.enabled = false;
        a.pressed_buttons.clear();
        a.pressed_keys.clear();
        a.keyboard_focus.reset();
        // bulk disable does not require setting alive=false; ops already reject !enabled
    }
}

void MultiAgentSession::run_schedule(const std::vector<SessionStep>& steps) {
    for (const auto& step : steps) {
        switch (step.op) {
            case SessionOp::Move:
                move(step.agent, step.pos);
                break;
            case SessionOp::PressKey:
                press_key(step.agent, step.keycode);
                break;
            case SessionOp::ReleaseKey:
                release_key(step.agent, step.keycode);
                break;
            case SessionOp::Focus:
                focus(step.agent, step.surface);
                break;
            case SessionOp::HostMove:
                move_host(step.pos);
                break;
            case SessionOp::HostFocus:
                focus_host(step.surface);
                break;
        }
    }
}

std::vector<SessionStep> MultiAgentSession::make_independent_workflows(
    const std::vector<std::string>& names,
    const std::vector<Vec2>& positions,
    const std::vector<std::uint32_t>& keycodes,
    const std::vector<std::string>& surfaces) {
    if (names.size() != positions.size() || names.size() != keycodes.size() ||
        names.size() != surfaces.size()) {
        throw std::invalid_argument("workflow vector sizes must match");
    }

    // Round-robin by phase: all moves, then all press_keys, then all focuses.
    // Models parallel agent steps with a deterministic total order.
    std::vector<SessionStep> out;
    out.reserve(names.size() * 3);

    for (std::size_t i = 0; i < names.size(); ++i)
        out.push_back(SessionStep{SessionOp::Move, names[i], positions[i], 0, {}});
    for (std::size_t i = 0; i < names.size(); ++i)
        out.push_back(SessionStep{SessionOp::PressKey, names[i], {}, keycodes[i], {}});
    for (std::size_t i = 0; i < names.size(); ++i)
        out.push_back(SessionStep{SessionOp::Focus, names[i], {}, 0, surfaces[i]});

    return out;
}

HostSnapshot MultiAgentSession::snapshot_host() const {
    HostSnapshot snap;
    snap.cursor          = host_.cursor;
    snap.keyboard_focus  = host_.keyboard_focus;
    snap.name            = host_.name;
    return snap;
}

bool MultiAgentSession::host_unchanged_since(const HostSnapshot& snap) const {
    return host_.name == snap.name && host_.cursor == snap.cursor &&
           host_.keyboard_focus == snap.keyboard_focus;
}

} // namespace hypr_agent_cursors
