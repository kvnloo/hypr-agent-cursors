#pragma once

#include <string>

namespace hypr_agent_cursors {

struct LoadRequest {
    std::string target_signature;
    std::string target_wayland;
    std::string plugin_path;
};

struct OutputRequest {
    std::string target_signature;
    std::string name;
};

struct GuardDecision {
    bool allowed = false;
    std::string reason;
};

class LiveSessionGuard {
  public:
    std::string live_signature;
    std::string live_wayland;

    GuardDecision may_load_plugin(const LoadRequest& req) const;
    GuardDecision may_create_output(const OutputRequest& req) const;
};

} // namespace hypr_agent_cursors
