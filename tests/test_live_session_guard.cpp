#include "hypr_agent_cursors/live_session_guard.hpp"

#include <gtest/gtest.h>

using hypr_agent_cursors::LiveSessionGuard;

TEST(LiveSessionGuard, RefusesWhenTargetSignatureEqualsLiveSignature) {
    LiveSessionGuard g;
    g.live_signature = "live-efb50993";
    g.live_wayland   = "wayland-1";

    auto decision = g.may_load_plugin({
        .target_signature = "live-efb50993",
        .target_wayland   = "wayland-1",
        .plugin_path      = "/tmp/hypr-agent-cursors.so",
    });

    EXPECT_FALSE(decision.allowed);
    EXPECT_EQ(decision.reason, "refusing to load into the live human seat");
}

TEST(LiveSessionGuard, AllowsNestedInstanceWithDifferentSignature) {
    LiveSessionGuard g;
    g.live_signature = "live-efb50993";
    g.live_wayland   = "wayland-1";

    auto decision = g.may_load_plugin({
        .target_signature = "nested-aaaaaaaa",
        .target_wayland   = "wayland-2",
        .plugin_path      = "/tmp/hypr-agent-cursors.so",
    });

    EXPECT_TRUE(decision.allowed);
}

TEST(LiveSessionGuard, RefusesEmptyTarget) {
    LiveSessionGuard g;
    g.live_signature = "live-efb50993";

    auto decision = g.may_load_plugin({
        .target_signature = "",
        .target_wayland   = "",
        .plugin_path      = "/tmp/hypr-agent-cursors.so",
    });

    EXPECT_FALSE(decision.allowed);
}

TEST(LiveSessionGuard, RefusesCreatingHeadlessOnLiveInstance) {
    LiveSessionGuard g;
    g.live_signature = "live-efb50993";

    auto decision = g.may_create_output({
        .target_signature = "live-efb50993",
        .name             = "HEADLESS-agent1",
    });

    EXPECT_FALSE(decision.allowed);
    EXPECT_EQ(decision.reason, "refusing to mutate live outputs");
}
