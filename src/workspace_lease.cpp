#include "hypr_agent_cursors/workspace_lease.hpp"

#include <utility>

namespace hypr_agent_cursors {

WorkspaceLeaseRegistry::WorkspaceLeaseRegistry(LiveSessionGuard guard)
    : guard_(std::move(guard)) {}

bool WorkspaceLeaseRegistry::owns_resources(LeaseState state) {
    return state != LeaseState::Released;
}

void WorkspaceLeaseRegistry::validate_request(const WorkspaceLeaseRequest& request) const {
    if (request.id.empty())
        throw std::invalid_argument("lease id must not be empty");
    if (request.output.empty())
        throw std::invalid_argument("lease output must not be empty");
    if (request.workspace <= 0)
        throw std::invalid_argument("lease workspace must be positive");
    const auto decision = guard_.may_create_output({
        .target_signature = request.compositor_signature,
        .name = request.output,
    });
    if (!decision.allowed)
        throw std::invalid_argument(decision.reason);
}

WorkspaceLease& WorkspaceLeaseRegistry::acquire(WorkspaceLeaseRequest request) {
    validate_request(request);

    auto existing = leases_.find(request.id);
    if (existing != leases_.end() && owns_resources(existing->second.state))
        throw std::invalid_argument("lease id already owns resources");

    if (output_owned(request.compositor_signature, request.output))
        throw std::invalid_argument("lease output already owned");
    if (workspace_owned(request.compositor_signature, request.workspace))
        throw std::invalid_argument("lease workspace already owned");

    WorkspaceLease next;
    next.id = std::move(request.id);
    next.compositor_signature = std::move(request.compositor_signature);
    next.output = std::move(request.output);
    next.workspace = request.workspace;
    next.state = LeaseState::Acquiring;

    const auto key = next.id;
    auto [it, _] = leases_.insert_or_assign(key, std::move(next));
    return it->second;
}

WorkspaceLease& WorkspaceLeaseRegistry::lease(const std::string& id) {
    auto it = leases_.find(id);
    if (it == leases_.end())
        throw std::out_of_range("unknown lease");
    return it->second;
}

const WorkspaceLease& WorkspaceLeaseRegistry::lease(const std::string& id) const {
    auto it = leases_.find(id);
    if (it == leases_.end())
        throw std::out_of_range("unknown lease");
    return it->second;
}

WorkspaceLease& WorkspaceLeaseRegistry::require_state(const std::string& id, LeaseState expected) {
    auto& value = lease(id);
    if (value.state != expected)
        throw std::invalid_argument("invalid lease state transition");
    return value;
}

void WorkspaceLeaseRegistry::mark_ready(const std::string& id) {
    require_state(id, LeaseState::Acquiring).state = LeaseState::Ready;
}

void WorkspaceLeaseRegistry::mark_failed(const std::string& id) {
    auto& value = lease(id);
    if (value.state != LeaseState::Acquiring && value.state != LeaseState::Ready)
        throw std::invalid_argument("invalid lease state transition");
    value.state = LeaseState::Failed;
}

void WorkspaceLeaseRegistry::attach_process(const std::string& id, std::int64_t pid) {
    auto& value = lease(id);
    if (value.state != LeaseState::Acquiring && value.state != LeaseState::Ready)
        throw std::invalid_argument("cannot attach process to inactive lease");
    if (pid <= 0)
        throw std::invalid_argument("process id must be positive");
    value.process_ids.insert(pid);
}

void WorkspaceLeaseRegistry::begin_release(const std::string& id) {
    auto& value = lease(id);
    if (value.state != LeaseState::Acquiring &&
        value.state != LeaseState::Ready &&
        value.state != LeaseState::Failed)
        throw std::invalid_argument("invalid lease state transition");
    value.state = LeaseState::Releasing;
}

void WorkspaceLeaseRegistry::release(const std::string& id) {
    auto& value = require_state(id, LeaseState::Releasing);
    value.process_ids.clear();
    value.state = LeaseState::Released;
}

bool WorkspaceLeaseRegistry::output_owned(
    const std::string& compositor_signature,
    const std::string& output) const {
    for (const auto& [_, value] : leases_) {
        if (owns_resources(value.state) &&
            value.compositor_signature == compositor_signature &&
            value.output == output)
            return true;
    }
    return false;
}

bool WorkspaceLeaseRegistry::workspace_owned(
    const std::string& compositor_signature,
    int workspace) const {
    for (const auto& [_, value] : leases_) {
        if (owns_resources(value.state) &&
            value.compositor_signature == compositor_signature &&
            value.workspace == workspace)
            return true;
    }
    return false;
}


} // namespace hypr_agent_cursors
