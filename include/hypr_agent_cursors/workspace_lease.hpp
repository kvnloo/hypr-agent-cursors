#pragma once

#include "hypr_agent_cursors/live_session_guard.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace hypr_agent_cursors {

enum class LeaseState {
    Acquiring,
    Ready,
    Releasing,
    Released,
    Failed,
};

struct WorkspaceLeaseRequest {
    std::string id;
    std::string compositor_signature;
    std::string output;
    int workspace = 0;
};

struct WorkspaceLease {
    std::string id;
    std::string compositor_signature;
    std::string output;
    int workspace = 0;
    LeaseState state = LeaseState::Acquiring;
    std::unordered_set<std::int64_t> process_ids;
};

class WorkspaceLeaseRegistry {
  public:
    explicit WorkspaceLeaseRegistry(LiveSessionGuard guard);

    WorkspaceLease& acquire(WorkspaceLeaseRequest request);
    WorkspaceLease& lease(const std::string& id);
    const WorkspaceLease& lease(const std::string& id) const;

    void mark_ready(const std::string& id);
    void mark_failed(const std::string& id);
    void attach_process(const std::string& id, std::int64_t pid);
    void begin_release(const std::string& id);
    void release(const std::string& id);

    bool output_owned(const std::string& compositor_signature, const std::string& output) const;
    bool workspace_owned(const std::string& compositor_signature, int workspace) const;

  private:
    static bool owns_resources(LeaseState state);
    WorkspaceLease& require_state(const std::string& id, LeaseState expected);
    void validate_request(const WorkspaceLeaseRequest& request) const;

    LiveSessionGuard guard_;
    std::unordered_map<std::string, WorkspaceLease> leases_;
};

} // namespace hypr_agent_cursors
