#include "hypr_agent_cursors/virtual_monitors.hpp"

#include <gtest/gtest.h>

using hypr_agent_cursors::AgentCursorRegistry;
using hypr_agent_cursors::LiveOutputPolicy;
using hypr_agent_cursors::Vec2;
using hypr_agent_cursors::VirtualMonitor;
using hypr_agent_cursors::VirtualMonitorRegistry;

// Policy: move_agent CLAMPS cursor coordinates to [0, width) x [0, height)
// of the agent's bound monitor. Out-of-range input is not an error; the
// stored cursor is always inside half-open pixel bounds.

TEST(VirtualMonitorRegistry, RegisterStoresNameSizeAndHeadlessFlag) {
    VirtualMonitorRegistry mon;
    mon.register_monitor({"HEADLESS-AGENT-1", 1920, 1080, true});
    mon.register_monitor({"eDP-1", 2560, 1600, false});

    const VirtualMonitor& h = mon.get("HEADLESS-AGENT-1");
    EXPECT_EQ(h.name, "HEADLESS-AGENT-1");
    EXPECT_EQ(h.width, 1920);
    EXPECT_EQ(h.height, 1080);
    EXPECT_TRUE(h.is_headless);

    const VirtualMonitor& live = mon.get("eDP-1");
    EXPECT_EQ(live.name, "eDP-1");
    EXPECT_FALSE(live.is_headless);
}

TEST(VirtualMonitorRegistry, UnknownMonitorThrows) {
    VirtualMonitorRegistry mon;
    EXPECT_THROW(mon.get("missing"), std::out_of_range);
}

TEST(AgentCursorRegistry, BoundOutputMustReferenceKnownMonitor) {
    VirtualMonitorRegistry mon;
    mon.register_monitor({"HEADLESS-AGENT-1", 800, 600, true});
    AgentCursorRegistry agents(mon);

    EXPECT_NO_THROW(agents.create("a1", "HEADLESS-AGENT-1"));
    EXPECT_THROW(agents.create("a2", "NOT-REGISTERED"), std::invalid_argument);
}

TEST(AgentCursorRegistry, MoveAgentClampsToMonitorBounds) {
    VirtualMonitorRegistry mon;
    mon.register_monitor({"HEADLESS-AGENT-1", 100, 50, true});
    AgentCursorRegistry agents(mon);
    agents.create("a1", "HEADLESS-AGENT-1");

    agents.move_agent("a1", {150.0, 75.0});
    EXPECT_EQ(agents.agent("a1").cursor.x, 99.0);  // width-1 inclusive clamp
    EXPECT_EQ(agents.agent("a1").cursor.y, 49.0);

    agents.move_agent("a1", {-10.0, -5.0});
    EXPECT_EQ(agents.agent("a1").cursor.x, 0.0);
    EXPECT_EQ(agents.agent("a1").cursor.y, 0.0);

    agents.move_agent("a1", {40.0, 20.0});
    EXPECT_EQ(agents.agent("a1").cursor.x, 40.0);
    EXPECT_EQ(agents.agent("a1").cursor.y, 20.0);
}

TEST(AgentCursorRegistry, TwoAgentsOnSameHeadlessHaveIndependentCursors) {
    VirtualMonitorRegistry mon;
    mon.register_monitor({"HEADLESS-AGENT-1", 1920, 1080, true});
    AgentCursorRegistry agents(mon);

    agents.create("agent-a", "HEADLESS-AGENT-1");
    agents.create("agent-b", "HEADLESS-AGENT-1");

    agents.move_agent("agent-a", {10.0, 20.0});
    agents.move_agent("agent-b", {30.0, 40.0});

    EXPECT_EQ(agents.agent("agent-a").bound_output, "HEADLESS-AGENT-1");
    EXPECT_EQ(agents.agent("agent-b").bound_output, "HEADLESS-AGENT-1");
    EXPECT_EQ(agents.agent("agent-a").cursor.x, 10.0);
    EXPECT_EQ(agents.agent("agent-a").cursor.y, 20.0);
    EXPECT_EQ(agents.agent("agent-b").cursor.x, 30.0);
    EXPECT_EQ(agents.agent("agent-b").cursor.y, 40.0);

    agents.move_agent("agent-a", {11.0, 21.0});
    EXPECT_EQ(agents.agent("agent-b").cursor.x, 30.0);
    EXPECT_EQ(agents.agent("agent-b").cursor.y, 40.0);
}

TEST(LiveOutputPolicy, DenylistRejectsBindingToLiveOutputNames) {
    LiveOutputPolicy policy;
    policy.deny("eDP-1");
    policy.deny("HDMI-A-1");

    EXPECT_FALSE(policy.allows_bind("eDP-1"));
    EXPECT_FALSE(policy.allows_bind("HDMI-A-1"));
    EXPECT_TRUE(policy.allows_bind("HEADLESS-A"));
    EXPECT_TRUE(policy.allows_bind("HEADLESS-AGENT-1"));
}

TEST(AgentCursorRegistry, CreateRespectsLiveOutputPolicyDenylist) {
    VirtualMonitorRegistry mon;
    mon.register_monitor({"HEADLESS-A", 1280, 720, true});
    mon.register_monitor({"eDP-1", 2560, 1600, false});

    LiveOutputPolicy policy;
    policy.deny("eDP-1");

    AgentCursorRegistry agents(mon, policy);

    EXPECT_NO_THROW(agents.create("on-headless", "HEADLESS-A"));
    EXPECT_THROW(agents.create("on-live", "eDP-1"), std::invalid_argument);

    EXPECT_EQ(agents.agent("on-headless").bound_output, "HEADLESS-A");
}

TEST(AgentCursorRegistry, AgentOnHeadlessACannotBindToLiveEdp1) {
    // Explicit scenario from the brief: agent intended for HEADLESS-A must not
    // be creatable bound to eDP-1 when live bind is forbidden.
    VirtualMonitorRegistry mon;
    mon.register_monitor({"HEADLESS-A", 1024, 768, true});
    mon.register_monitor({"eDP-1", 1920, 1080, false});

    LiveOutputPolicy policy;
    policy.deny("eDP-1");

    AgentCursorRegistry agents(mon, policy);
    EXPECT_THROW(agents.create("rogue", "eDP-1"), std::invalid_argument);
}
