#include "hypr_agent_cursors/multi_agent_session.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using hypr_agent_cursors::HostSnapshot;
using hypr_agent_cursors::MultiAgentSession;
using hypr_agent_cursors::OutputBindingMode;
using hypr_agent_cursors::SessionOp;
using hypr_agent_cursors::SessionStep;
using hypr_agent_cursors::Vec2;

namespace {

HostSnapshot freeze_host(const MultiAgentSession& s) {
    return s.snapshot_host();
}

} // namespace

TEST(MultiAgentSession, HostSeatIsAlwaysSeat0) {
    MultiAgentSession s;
    EXPECT_EQ(s.host().name, "seat0");
    s.create_agents(3, OutputBindingMode::PerAgent);
    EXPECT_EQ(s.host().name, "seat0");
    for (const auto& n : s.agent_names())
        EXPECT_NE(n, "seat0");
}

TEST(MultiAgentSession, CreateNAgentsBoundToHeadlessI) {
    MultiAgentSession s;
    s.create_agents(4, OutputBindingMode::PerAgent);

    ASSERT_EQ(s.agent_count(), 4u);
    auto names = s.agent_names();
    ASSERT_EQ(names.size(), 4u);
    EXPECT_EQ(names[0], "agent-1");
    EXPECT_EQ(names[1], "agent-2");
    EXPECT_EQ(names[2], "agent-3");
    EXPECT_EQ(names[3], "agent-4");

    EXPECT_EQ(s.agent("agent-1").bound_output, "HEADLESS-1");
    EXPECT_EQ(s.agent("agent-2").bound_output, "HEADLESS-2");
    EXPECT_EQ(s.agent("agent-3").bound_output, "HEADLESS-3");
    EXPECT_EQ(s.agent("agent-4").bound_output, "HEADLESS-4");
    for (const auto& n : names) {
        EXPECT_TRUE(s.agent(n).enabled);
        EXPECT_TRUE(s.agent(n).alive);
    }
}

TEST(MultiAgentSession, CreateNAgentsBoundToSharedHeadlessPool) {
    MultiAgentSession s;
    s.create_agents(3, OutputBindingMode::SharedPool);

    ASSERT_EQ(s.agent_count(), 3u);
    EXPECT_EQ(s.agent("agent-1").bound_output, "HEADLESS-POOL");
    EXPECT_EQ(s.agent("agent-2").bound_output, "HEADLESS-POOL");
    EXPECT_EQ(s.agent("agent-3").bound_output, "HEADLESS-POOL");
    // Agents remain independent seats even on shared pool output.
    EXPECT_EQ(s.agent("agent-1").name, "agent-1");
    EXPECT_EQ(s.agent("agent-2").name, "agent-2");
}

TEST(MultiAgentSession, AgentOpsNeverMutateHostUnlessExplicitHostOp) {
    MultiAgentSession s;
    s.host().cursor = {111.0, 222.0};
    s.host().keyboard_focus = "human-surface";
    s.create_agents(2, OutputBindingMode::PerAgent);

    const auto snap = freeze_host(s);

    s.move("agent-1", {10.0, 20.0});
    s.press_key("agent-1", 30);
    s.focus("agent-1", "surface-a");
    s.move("agent-2", {30.0, 40.0});
    s.press_key("agent-2", 31);
    s.focus("agent-2", "surface-b");

    EXPECT_TRUE(s.host_unchanged_since(snap));
    EXPECT_EQ(s.host().cursor.x, 111.0);
    EXPECT_EQ(s.host().cursor.y, 222.0);
    EXPECT_EQ(s.host().keyboard_focus, "human-surface");

    s.move_host({9.0, 8.0});
    EXPECT_FALSE(s.host_unchanged_since(snap));
    EXPECT_EQ(s.host().cursor.x, 9.0);
    EXPECT_EQ(s.host().cursor.y, 8.0);
    // Agents untouched by host move.
    EXPECT_EQ(s.agent("agent-1").cursor.x, 10.0);
    EXPECT_EQ(s.agent("agent-2").cursor.x, 30.0);
}

TEST(MultiAgentSession, DeterministicInterleavingPreservesHostInvariant) {
    MultiAgentSession s;
    s.host().cursor = {50.0, 60.0};
    s.host().keyboard_focus = "desk";
    s.create_agents(3, OutputBindingMode::PerAgent);
    const auto snap = freeze_host(s);

    // Explicit interleaving of three concurrent workflows (not threads).
    std::vector<SessionStep> schedule = {
        {SessionOp::Move, "agent-1", {1.0, 1.0}, 0, {}},
        {SessionOp::Move, "agent-2", {2.0, 2.0}, 0, {}},
        {SessionOp::Move, "agent-3", {3.0, 3.0}, 0, {}},
        {SessionOp::PressKey, "agent-2", {}, 16, {}},
        {SessionOp::PressKey, "agent-1", {}, 30, {}},
        {SessionOp::PressKey, "agent-3", {}, 17, {}},
        {SessionOp::Focus, "agent-3", {}, 0, "surf-3"},
        {SessionOp::Focus, "agent-1", {}, 0, "surf-1"},
        {SessionOp::Focus, "agent-2", {}, 0, "surf-2"},
    };
    s.run_schedule(schedule);

    EXPECT_TRUE(s.host_unchanged_since(snap));
    EXPECT_EQ(s.agent("agent-1").cursor, (Vec2{1.0, 1.0}));
    EXPECT_EQ(s.agent("agent-2").cursor, (Vec2{2.0, 2.0}));
    EXPECT_EQ(s.agent("agent-3").cursor, (Vec2{3.0, 3.0}));
    EXPECT_TRUE(s.agent("agent-1").pressed_keys.count(30));
    EXPECT_TRUE(s.agent("agent-2").pressed_keys.count(16));
    EXPECT_TRUE(s.agent("agent-3").pressed_keys.count(17));
    EXPECT_EQ(s.agent("agent-1").keyboard_focus, "surf-1");
    EXPECT_EQ(s.agent("agent-2").keyboard_focus, "surf-2");
    EXPECT_EQ(s.agent("agent-3").keyboard_focus, "surf-3");
}

TEST(MultiAgentSession, KillOneAgentDoesNotAffectOthers) {
    MultiAgentSession s;
    s.create_agents(3, OutputBindingMode::SharedPool);
    s.move("agent-1", {1.0, 1.0});
    s.move("agent-2", {2.0, 2.0});
    s.move("agent-3", {3.0, 3.0});
    s.press_key("agent-1", 10);
    s.press_key("agent-2", 20);
    s.press_key("agent-3", 30);
    s.focus("agent-1", "a");
    s.focus("agent-2", "b");
    s.focus("agent-3", "c");

    s.kill("agent-2");

    EXPECT_FALSE(s.agent("agent-2").alive);
    EXPECT_FALSE(s.agent("agent-2").enabled);
    EXPECT_TRUE(s.agent("agent-2").pressed_keys.empty());
    EXPECT_FALSE(s.agent("agent-2").keyboard_focus.has_value());

    // Survivors still independent and operable.
    EXPECT_TRUE(s.agent("agent-1").alive);
    EXPECT_TRUE(s.agent("agent-3").alive);
    EXPECT_EQ(s.agent("agent-1").cursor.x, 1.0);
    EXPECT_EQ(s.agent("agent-3").cursor.x, 3.0);
    EXPECT_TRUE(s.agent("agent-1").pressed_keys.count(10));
    EXPECT_TRUE(s.agent("agent-3").pressed_keys.count(30));

    s.move("agent-1", {100.0, 100.0});
    s.press_key("agent-3", 99);
    s.focus("agent-1", "a2");
    EXPECT_EQ(s.agent("agent-1").cursor.x, 100.0);
    EXPECT_TRUE(s.agent("agent-3").pressed_keys.count(99));
    EXPECT_EQ(s.agent("agent-1").keyboard_focus, "a2");

    EXPECT_THROW(s.move("agent-2", {0.0, 0.0}), std::invalid_argument);
    EXPECT_THROW(s.press_key("agent-2", 1), std::invalid_argument);
}

TEST(MultiAgentSession, BulkDisableAllKillSwitch) {
    MultiAgentSession s;
    s.host().cursor = {7.0, 8.0};
    s.host().keyboard_focus = "human";
    s.create_agents(4, OutputBindingMode::PerAgent);
    const auto snap = freeze_host(s);

    for (const auto& n : s.agent_names()) {
        s.move(n, {1.0, 1.0});
        s.press_key(n, 42);
        s.focus(n, "surf-" + n);
    }

    s.bulk_disable_all();

    EXPECT_TRUE(s.host_unchanged_since(snap));
    for (const auto& n : s.agent_names()) {
        EXPECT_FALSE(s.agent(n).enabled);
        EXPECT_TRUE(s.agent(n).pressed_keys.empty());
        EXPECT_TRUE(s.agent(n).pressed_buttons.empty());
        EXPECT_FALSE(s.agent(n).keyboard_focus.has_value());
        EXPECT_THROW(s.move(n, {0.0, 0.0}), std::invalid_argument);
        EXPECT_THROW(s.press_key(n, 1), std::invalid_argument);
        EXPECT_THROW(s.focus(n, "x"), std::invalid_argument);
    }
}

TEST(MultiAgentSession, FourAgentsIndependentWorkflows) {
    MultiAgentSession s;
    s.host().cursor = {640.0, 360.0};
    s.host().keyboard_focus = "human-desk";
    s.create_agents(4, OutputBindingMode::PerAgent);
    const auto snap = freeze_host(s);

    const auto names = s.agent_names();
    const std::vector<Vec2> positions = {
        {10.0, 10.0},
        {20.0, 20.0},
        {30.0, 30.0},
        {40.0, 40.0},
    };
    const std::vector<std::uint32_t> keys = {16, 17, 18, 19}; // q w e r
    const std::vector<std::string> surfaces = {
        "term-agent-1",
        "browser-agent-2",
        "editor-agent-3",
        "mail-agent-4",
    };

    // Round-robin interleaving models parallel computer-use without threads.
    auto schedule = MultiAgentSession::make_independent_workflows(names, positions, keys, surfaces);
    ASSERT_EQ(schedule.size(), 12u); // 4 agents * (move + press + focus)

    s.run_schedule(schedule);

    // Host invariant after any interleaving.
    EXPECT_TRUE(s.host_unchanged_since(snap));
    EXPECT_EQ(s.host().name, "seat0");
    EXPECT_EQ(s.host().cursor.x, 640.0);
    EXPECT_EQ(s.host().cursor.y, 360.0);
    EXPECT_EQ(s.host().keyboard_focus, "human-desk");

    for (std::size_t i = 0; i < 4; ++i) {
        const auto& n = names[i];
        EXPECT_EQ(s.agent(n).bound_output, "HEADLESS-" + std::to_string(i + 1));
        EXPECT_EQ(s.agent(n).cursor, positions[i]);
        EXPECT_TRUE(s.agent(n).pressed_keys.count(keys[i]));
        EXPECT_EQ(s.agent(n).keyboard_focus, surfaces[i]);
        EXPECT_TRUE(s.agent(n).enabled);
        EXPECT_TRUE(s.agent(n).alive);
    }

    // Cross-agent independence: agent-1 focus/key did not leak.
    EXPECT_FALSE(s.agent("agent-2").pressed_keys.count(keys[0]));
    EXPECT_NE(s.agent("agent-1").keyboard_focus, s.agent("agent-2").keyboard_focus);

    // Kill agent-3 mid-session; others continue.
    s.kill("agent-3");
    EXPECT_FALSE(s.agent("agent-3").alive);
    s.move("agent-1", {99.0, 99.0});
    s.press_key("agent-4", 20);
    EXPECT_EQ(s.agent("agent-1").cursor.x, 99.0);
    EXPECT_TRUE(s.agent("agent-4").pressed_keys.count(20));
    EXPECT_TRUE(s.host_unchanged_since(snap));

    // Bulk kill switch ends the rest without host mutation.
    s.bulk_disable_all();
    EXPECT_TRUE(s.host_unchanged_since(snap));
    for (const auto& n : names) {
        EXPECT_FALSE(s.agent(n).enabled);
        EXPECT_TRUE(s.agent(n).pressed_keys.empty());
    }
}

TEST(MultiAgentSession, ReleaseKeyAndDisabledReject) {
    MultiAgentSession s;
    s.create_agents(1, OutputBindingMode::PerAgent);
    s.press_key("agent-1", 30);
    EXPECT_TRUE(s.agent("agent-1").pressed_keys.count(30));
    s.release_key("agent-1", 30);
    EXPECT_FALSE(s.agent("agent-1").pressed_keys.count(30));

    s.bulk_disable_all();
    EXPECT_THROW(s.release_key("agent-1", 30), std::invalid_argument);
}

TEST(MultiAgentSession, CreateAgentsRejectsZeroAndDuplicateSessionReuse) {
    MultiAgentSession s;
    EXPECT_THROW(s.create_agents(0, OutputBindingMode::PerAgent), std::invalid_argument);
    s.create_agents(2, OutputBindingMode::PerAgent);
    EXPECT_THROW(s.create_agents(1, OutputBindingMode::PerAgent), std::invalid_argument);
}
