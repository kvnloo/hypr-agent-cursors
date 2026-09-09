#include "hypr_agent_cursors/agent_seats.hpp"

#include <gtest/gtest.h>

using hypr_agent_cursors::AgentSeatRegistry;
using hypr_agent_cursors::Vec2;

TEST(AgentSeatRegistry, CreateDoesNotMoveHostCursor) {
    AgentSeatRegistry r;
    r.host().cursor = {120.0, 80.0};
    r.host().keyboard_focus = "kitty:human";

    auto& agent = r.create("agent1", "HEADLESS-agent1");

    EXPECT_EQ(r.host().cursor.x, 120.0);
    EXPECT_EQ(r.host().cursor.y, 80.0);
    EXPECT_EQ(r.host().keyboard_focus, "kitty:human");
    EXPECT_EQ(agent.name, "agent1");
    EXPECT_EQ(agent.bound_output, "HEADLESS-agent1");
    EXPECT_TRUE(agent.enabled);
}

TEST(AgentSeatRegistry, MoveAgentLeavesHostCursorAlone) {
    AgentSeatRegistry r;
    r.host().cursor = {10.0, 10.0};
    r.create("agent1", "HEADLESS-agent1");

    r.move_agent("agent1", {400.0, 300.0});

    EXPECT_EQ(r.agent("agent1").cursor.x, 400.0);
    EXPECT_EQ(r.agent("agent1").cursor.y, 300.0);
    EXPECT_EQ(r.host().cursor.x, 10.0);
    EXPECT_EQ(r.host().cursor.y, 10.0);
}

TEST(AgentSeatRegistry, MoveHostLeavesAgentCursorAlone) {
    AgentSeatRegistry r;
    r.create("agent1", "HEADLESS-agent1");
    r.move_agent("agent1", {50.0, 60.0});

    r.move_host({9.0, 8.0});

    EXPECT_EQ(r.host().cursor.x, 9.0);
    EXPECT_EQ(r.host().cursor.y, 8.0);
    EXPECT_EQ(r.agent("agent1").cursor.x, 50.0);
    EXPECT_EQ(r.agent("agent1").cursor.y, 60.0);
}

TEST(AgentSeatRegistry, TwoAgentsHaveIndependentCursorsAndFocus) {
    AgentSeatRegistry r;
    r.create("agent1", "HEADLESS-agent1");
    r.create("agent2", "HEADLESS-agent2");

    r.move_agent("agent1", {1.0, 2.0});
    r.move_agent("agent2", {3.0, 4.0});
    r.focus_agent_keyboard("agent1", "term-a");
    r.focus_agent_keyboard("agent2", "term-b");
    r.host().keyboard_focus = "term-human";

    EXPECT_EQ(r.agent("agent1").cursor.x, 1.0);
    EXPECT_EQ(r.agent("agent2").cursor.x, 3.0);
    EXPECT_EQ(r.agent("agent1").keyboard_focus, "term-a");
    EXPECT_EQ(r.agent("agent2").keyboard_focus, "term-b");
    EXPECT_EQ(r.host().keyboard_focus, "term-human");
}

TEST(AgentSeatRegistry, HostSeatNameIsNeverReplaced) {
    AgentSeatRegistry r;
    r.create("agent1", "HEADLESS-agent1");
    EXPECT_EQ(r.host().name, "seat0");
    EXPECT_NE(r.agent("agent1").name, r.host().name);
}

TEST(AgentSeatRegistry, KillSwitchDisablesAgentAndReleasesButtonsAndKeys) {
    AgentSeatRegistry r;
    auto& agent = r.create("agent1", "HEADLESS-agent1");
    agent.pressed_buttons = {272};
    agent.pressed_keys    = {30, 31};
    agent.keyboard_focus  = "term-a";

    r.disable("agent1");

    EXPECT_FALSE(r.agent("agent1").enabled);
    EXPECT_TRUE(r.agent("agent1").pressed_buttons.empty());
    EXPECT_TRUE(r.agent("agent1").pressed_keys.empty());
    EXPECT_FALSE(r.agent("agent1").keyboard_focus.has_value());
}

TEST(AgentSeatRegistry, UnknownAgentIsAnErrorNotAHostMutation) {
    AgentSeatRegistry r;
    r.host().cursor = {1.0, 1.0};
    EXPECT_THROW(r.move_agent("missing", {9.0, 9.0}), std::out_of_range);
    EXPECT_EQ(r.host().cursor.x, 1.0);
}

TEST(AgentSeatRegistry, DuplicateNameIsRejected) {
    AgentSeatRegistry r;
    r.create("agent1", "HEADLESS-agent1");
    EXPECT_THROW(r.create("agent1", "HEADLESS-other"), std::invalid_argument);
}

TEST(AgentSeatRegistry, AgentMotionNeverReportsAsHostMotion) {
    AgentSeatRegistry r;
    r.host().cursor = {0.0, 0.0};
    r.create("agent1", "HEADLESS-agent1");
    const auto host_before = r.host().cursor;
    r.move_agent("agent1", {1280.0, 720.0});
    EXPECT_EQ(r.host().cursor.x, host_before.x);
    EXPECT_EQ(r.host().cursor.y, host_before.y);
    EXPECT_FALSE(r.last_host_cursor_moved());
}

TEST(AgentSeatRegistry, PressKeyAddsToAgentWithoutChangingHostFocus) {
    AgentSeatRegistry r;
    r.host().keyboard_focus = "kitty:human";
    r.create("agent1", "HEADLESS-agent1");
    r.focus_agent_keyboard("agent1", "term-a");

    r.press_key("agent1", 30);

    EXPECT_TRUE(r.agent("agent1").pressed_keys.contains(30));
    EXPECT_EQ(r.host().keyboard_focus, "kitty:human");
    EXPECT_EQ(r.agent("agent1").keyboard_focus, "term-a");
}

TEST(AgentSeatRegistry, ReleaseKeyRemovesKey) {
    AgentSeatRegistry r;
    r.create("agent1", "HEADLESS-agent1");
    r.press_key("agent1", 30);
    r.press_key("agent1", 31);

    r.release_key("agent1", 30);

    EXPECT_FALSE(r.agent("agent1").pressed_keys.contains(30));
    EXPECT_TRUE(r.agent("agent1").pressed_keys.contains(31));
}

TEST(AgentSeatRegistry, TwoAgentsHoldDifferentKeysSimultaneously) {
    AgentSeatRegistry r;
    r.create("agent1", "HEADLESS-agent1");
    r.create("agent2", "HEADLESS-agent2");

    r.press_key("agent1", 30);
    r.press_key("agent2", 42);

    EXPECT_TRUE(r.agent("agent1").pressed_keys.contains(30));
    EXPECT_FALSE(r.agent("agent1").pressed_keys.contains(42));
    EXPECT_TRUE(r.agent("agent2").pressed_keys.contains(42));
    EXPECT_FALSE(r.agent("agent2").pressed_keys.contains(30));
}

TEST(AgentSeatRegistry, DisableReleasesKeysPressedViaApi) {
    AgentSeatRegistry r;
    r.create("agent1", "HEADLESS-agent1");
    r.press_key("agent1", 30);
    r.press_key("agent1", 31);

    r.disable("agent1");

    EXPECT_FALSE(r.agent("agent1").enabled);
    EXPECT_TRUE(r.agent("agent1").pressed_keys.empty());
}

TEST(AgentSeatRegistry, PressKeyOnDisabledAgentErrors) {
    AgentSeatRegistry r;
    r.create("agent1", "HEADLESS-agent1");
    r.disable("agent1");

    EXPECT_THROW(r.press_key("agent1", 30), std::invalid_argument);
    EXPECT_TRUE(r.agent("agent1").pressed_keys.empty());
}

TEST(AgentSeatRegistry, HostPressKeyDoesNotAppearAsAgentKeys) {
    AgentSeatRegistry r;
    r.create("agent1", "HEADLESS-agent1");
    r.press_key("agent1", 30);

    r.host_press_key(16);

    EXPECT_TRUE(r.host().pressed_keys.contains(16));
    EXPECT_FALSE(r.agent("agent1").pressed_keys.contains(16));
    EXPECT_TRUE(r.agent("agent1").pressed_keys.contains(30));
    EXPECT_FALSE(r.host().pressed_keys.contains(30));
}
