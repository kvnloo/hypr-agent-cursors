#include "hypr_agent_cursors/live_session_guard.hpp"

namespace hypr_agent_cursors {

GuardDecision LiveSessionGuard::may_load_plugin(const LoadRequest& req) const {
    if (req.target_signature.empty() || req.target_wayland.empty())
        return {false, "refusing empty target compositor"};
    if (req.target_signature == live_signature || req.target_wayland == live_wayland)
        return {false, "refusing to load into the live human seat"};
    return {true, {}};
}

GuardDecision LiveSessionGuard::may_create_output(const OutputRequest& req) const {
    if (req.target_signature.empty())
        return {false, "refusing empty target compositor"};
    if (req.target_signature == live_signature)
        return {false, "refusing to mutate live outputs"};
    return {true, {}};
}

} // namespace hypr_agent_cursors
