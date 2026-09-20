#include "hypr_agent_cursors/workspace_lease.hpp"

#include <gtest/gtest.h>

#include <string>
#include <utility>

using hypr_agent_cursors::LeaseState;
using hypr_agent_cursors::LiveSessionGuard;
using hypr_agent_cursors::WorkspaceLeaseRegistry;
using hypr_agent_cursors::WorkspaceLeaseRequest;

namespace {

LiveSessionGuard nested_only_guard() {
    LiveSessionGuard guard;
    guard.live_signature = "human-his";
    guard.live_wayland = "wayland-1";
    return guard;
}

WorkspaceLeaseRequest request(
    std::string id,
    std::string signature,
    std::string output,
    int workspace) {
    return {
        .id = std::move(id),
        .compositor_signature = std::move(signature),
        .output = std::move(output),
        .workspace = workspace,
    };
}

} // namespace

TEST(WorkspaceLeaseRegistry, RefusesLeaseOnLiveHumanCompositor) {
    WorkspaceLeaseRegistry leases(nested_only_guard());

    EXPECT_THROW(
        leases.acquire(request("agent-a", "human-his", "AGENT-A", 91)),
        std::invalid_argument);
    EXPECT_FALSE(leases.output_owned("human-his", "AGENT-A"));
    EXPECT_FALSE(leases.workspace_owned("human-his", 91));
}

TEST(WorkspaceLeaseRegistry, AcquireStartsInAcquiringThenBecomesReady) {
    WorkspaceLeaseRegistry leases(nested_only_guard());

    auto& value = leases.acquire(request("agent-a", "nested-a", "AGENT-A", 91));
    EXPECT_EQ(value.state, LeaseState::Acquiring);

    leases.mark_ready("agent-a");
    EXPECT_EQ(leases.lease("agent-a").state, LeaseState::Ready);
}

TEST(WorkspaceLeaseRegistry, OutputAndWorkspaceOwnershipAreUniquePerCompositor) {
    WorkspaceLeaseRegistry leases(nested_only_guard());
    leases.acquire(request("agent-a", "nested-a", "AGENT-A", 91));

    EXPECT_THROW(
        leases.acquire(request("agent-b", "nested-a", "AGENT-A", 92)),
        std::invalid_argument);
    EXPECT_THROW(
        leases.acquire(request("agent-b", "nested-a", "AGENT-B", 91)),
        std::invalid_argument);

    // Different compositor instances have independent namespaces.
    EXPECT_NO_THROW(
        leases.acquire(request("agent-c", "nested-c", "AGENT-A", 91)));
}

TEST(WorkspaceLeaseRegistry, FailureRetainsOwnershipUntilExplicitRelease) {
    WorkspaceLeaseRegistry leases(nested_only_guard());
    leases.acquire(request("agent-a", "nested-a", "AGENT-A", 91));
    leases.attach_process("agent-a", 4242);
    leases.mark_failed("agent-a");

    EXPECT_TRUE(leases.output_owned("nested-a", "AGENT-A"));
    EXPECT_TRUE(leases.workspace_owned("nested-a", 91));
    EXPECT_TRUE(leases.lease("agent-a").process_ids.contains(4242));

    EXPECT_THROW(
        leases.acquire(request("agent-b", "nested-a", "AGENT-A", 92)),
        std::invalid_argument);

    leases.begin_release("agent-a");
    leases.release("agent-a");

    EXPECT_FALSE(leases.output_owned("nested-a", "AGENT-A"));
    EXPECT_FALSE(leases.workspace_owned("nested-a", 91));
    EXPECT_TRUE(leases.lease("agent-a").process_ids.empty());
    EXPECT_EQ(leases.lease("agent-a").state, LeaseState::Released);

    EXPECT_NO_THROW(
        leases.acquire(request("agent-b", "nested-a", "AGENT-A", 91)));
}

TEST(WorkspaceLeaseRegistry, ReleaseRequiresExplicitReleasingState) {
    WorkspaceLeaseRegistry leases(nested_only_guard());
    leases.acquire(request("agent-a", "nested-a", "AGENT-A", 91));
    leases.mark_ready("agent-a");

    EXPECT_THROW(leases.release("agent-a"), std::invalid_argument);

    leases.begin_release("agent-a");
    EXPECT_EQ(leases.lease("agent-a").state, LeaseState::Releasing);
    leases.release("agent-a");
    EXPECT_EQ(leases.lease("agent-a").state, LeaseState::Released);
}

TEST(WorkspaceLeaseRegistry, ParallelLeasesKeepProcessOwnershipIndependent) {
    WorkspaceLeaseRegistry leases(nested_only_guard());

    leases.acquire(request("agent-a", "nested-a", "AGENT-A", 91));
    leases.acquire(request("agent-b", "nested-b", "AGENT-B", 92));
    leases.attach_process("agent-a", 1001);
    leases.attach_process("agent-a", 1002);
    leases.attach_process("agent-b", 2001);
    leases.mark_ready("agent-a");
    leases.mark_ready("agent-b");

    EXPECT_TRUE(leases.lease("agent-a").process_ids.contains(1001));
    EXPECT_TRUE(leases.lease("agent-a").process_ids.contains(1002));
    EXPECT_FALSE(leases.lease("agent-a").process_ids.contains(2001));
    EXPECT_TRUE(leases.lease("agent-b").process_ids.contains(2001));

    leases.mark_failed("agent-a");
    EXPECT_EQ(leases.lease("agent-b").state, LeaseState::Ready);
    EXPECT_TRUE(leases.lease("agent-b").process_ids.contains(2001));
}

TEST(WorkspaceLeaseRegistry, InvalidRequestsFailBeforeOwningAnything) {
    WorkspaceLeaseRegistry leases(nested_only_guard());

    auto empty_id = request("", "nested-a", "AGENT-A", 91);
    EXPECT_THROW(leases.acquire(empty_id), std::invalid_argument);

    auto empty_output = request("agent-a", "nested-a", "", 91);
    EXPECT_THROW(leases.acquire(empty_output), std::invalid_argument);

    auto bad_workspace = request("agent-a", "nested-a", "AGENT-A", 0);
    EXPECT_THROW(leases.acquire(bad_workspace), std::invalid_argument);

    EXPECT_FALSE(leases.output_owned("nested-a", "AGENT-A"));
    EXPECT_FALSE(leases.workspace_owned("nested-a", 91));
}
