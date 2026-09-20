CXX ?= g++
CXXFLAGS ?= -std=c++23 -g -Wall -Wextra -Werror -Iinclude
GTEST_LIBS := $(shell pkg-config --libs gtest_main)
BUILD := build
HYPR_CFLAGS := $(shell pkg-config --cflags hyprland 2>/dev/null)

.PHONY: test test-seats test-vout test-parallel test-lease test-abi shim full plugin nm clean cli

# --- unit suites (separate binaries: each module owns its Vec2 in isolation) ---

$(BUILD)/hypr_agent_cursors_tests: src/agent_seats.cpp src/live_session_guard.cpp \
		tests/test_agent_seats.cpp tests/test_live_session_guard.cpp \
		include/hypr_agent_cursors/agent_seats.hpp include/hypr_agent_cursors/live_session_guard.hpp
	mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) src/agent_seats.cpp src/live_session_guard.cpp \
		tests/test_agent_seats.cpp tests/test_live_session_guard.cpp $(GTEST_LIBS) -o $@

$(BUILD)/virtual_monitor_tests: src/virtual_monitors.cpp tests/test_virtual_monitors.cpp \
		include/hypr_agent_cursors/virtual_monitors.hpp
	mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) src/virtual_monitors.cpp tests/test_virtual_monitors.cpp $(GTEST_LIBS) -o $@

$(BUILD)/multi_agent_session_tests: src/multi_agent_session.cpp tests/test_multi_agent_session.cpp \
		include/hypr_agent_cursors/multi_agent_session.hpp
	mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) src/multi_agent_session.cpp tests/test_multi_agent_session.cpp $(GTEST_LIBS) -o $@

$(BUILD)/workspace_lease_tests: src/workspace_lease.cpp src/live_session_guard.cpp \
		tests/test_workspace_lease.cpp \
		include/hypr_agent_cursors/workspace_lease.hpp include/hypr_agent_cursors/live_session_guard.hpp
	mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) src/workspace_lease.cpp src/live_session_guard.cpp \
		tests/test_workspace_lease.cpp $(GTEST_LIBS) -o $@

$(BUILD)/plugin_shim.so: plugin/shim.cpp
	mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -fPIC -shared -DHYPR_AGENT_CURSORS_TEST_SHIM=1 plugin/shim.cpp -o $@

$(BUILD)/test_abi: tests/test_abi.cpp
	mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) tests/test_abi.cpp -ldl -o $@

test-seats: $(BUILD)/hypr_agent_cursors_tests
	$(BUILD)/hypr_agent_cursors_tests --gtest_color=no

test-vout: $(BUILD)/virtual_monitor_tests
	$(BUILD)/virtual_monitor_tests --gtest_color=no

test-parallel: $(BUILD)/multi_agent_session_tests
	$(BUILD)/multi_agent_session_tests --gtest_color=no

test-lease: $(BUILD)/workspace_lease_tests
	$(BUILD)/workspace_lease_tests --gtest_color=no

test-abi: $(BUILD)/test_abi $(BUILD)/plugin_shim.so
	$(BUILD)/test_abi $(BUILD)/plugin_shim.so
	nm -D --defined-only $(BUILD)/plugin_shim.so | grep -E 'pluginAPIVersion|pluginInit|pluginExit'

test: test-seats test-vout test-parallel test-lease test-abi
	@echo 'ALL unit suites green (seats+kb, virtual-monitor, multi-agent, workspace-lease, plugin shim ABI)'

shim: $(BUILD)/plugin_shim.so

# Full plugin .so — links against headers; offline dlopen fails (needs compositor).
# NEVER hyprctl plugin load against the live human session.
full: plugin/main.cpp
	mkdir -p $(BUILD)
	$(CXX) -std=c++23 -fPIC -shared -O2 $(HYPR_CFLAGS) -I/usr/include/hyprland/src \
		plugin/main.cpp -o $(BUILD)/hypr_agent_cursors.so
	nm -D --defined-only $(BUILD)/hypr_agent_cursors.so | grep -E 'pluginAPIVersion|pluginInit|pluginExit' || true

plugin:
	@echo 'Use: make shim (offline ABI) or make full (compositor .so). Load ONLY in nested HIS.'
	@echo 'Wrap hyprctl with: HYPR_AGENT_LIVE_SIGNATURE=$$live scripts/never-touch-live.sh hyprctl ...'

nm: shim
	nm -D --defined-only $(BUILD)/plugin_shim.so

clean:
	rm -rf $(BUILD)
