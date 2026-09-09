// Small CLI that runs the 4-agent independent workflow scenario (no gtest).
// Exit 0 on success; prints PASS/FAIL lines for scripting.

#include "hypr_agent_cursors/multi_agent_session.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using hypr_agent_cursors::MultiAgentSession;
using hypr_agent_cursors::OutputBindingMode;
using hypr_agent_cursors::Vec2;

int main() {
    MultiAgentSession s;
    s.host().cursor = {640.0, 360.0};
    s.host().keyboard_focus = "human-desk";
    s.create_agents(4, OutputBindingMode::PerAgent);
    const auto snap = s.snapshot_host();

    const auto names = s.agent_names();
    const std::vector<Vec2> positions = {
        {10.0, 10.0}, {20.0, 20.0}, {30.0, 30.0}, {40.0, 40.0},
    };
    const std::vector<std::uint32_t> keys = {16, 17, 18, 19};
    const std::vector<std::string> surfaces = {
        "term-agent-1", "browser-agent-2", "editor-agent-3", "mail-agent-4",
    };

    auto schedule =
        MultiAgentSession::make_independent_workflows(names, positions, keys, surfaces);
    s.run_schedule(schedule);

    bool ok = true;
    auto check = [&](bool cond, const char* msg) {
        if (!cond) {
            std::cerr << "FAIL: " << msg << "\n";
            ok = false;
        }
    };

    check(s.host_unchanged_since(snap), "host invariant after interleaved workflows");
    check(s.host().name == "seat0", "host seat name is seat0");
    for (std::size_t i = 0; i < 4; ++i) {
        const auto& n = names[i];
        check(s.agent(n).cursor == positions[i], "agent cursor after workflow");
        check(s.agent(n).pressed_keys.count(keys[i]) == 1, "agent key pressed");
        check(s.agent(n).keyboard_focus == surfaces[i], "agent focus surface");
        check(s.agent(n).bound_output == ("HEADLESS-" + std::to_string(i + 1)),
              "per-agent HEADLESS binding");
    }

    s.kill("agent-3");
    check(!s.agent("agent-3").alive, "killed agent not alive");
    s.move("agent-1", {99.0, 99.0});
    check(s.agent("agent-1").cursor.x == 99.0, "survivor still movable after kill");
    check(s.host_unchanged_since(snap), "host invariant after kill");

    s.bulk_disable_all();
    check(s.host_unchanged_since(snap), "host invariant after bulk_disable_all");
    for (const auto& n : names)
        check(!s.agent(n).enabled, "all agents disabled");

    if (ok) {
        std::cout << "PASS MultiAgentSession.FourAgentsIndependentWorkflows\n";
        return 0;
    }
    std::cout << "FAIL MultiAgentSession.FourAgentsIndependentWorkflows\n";
    return 1;
}
