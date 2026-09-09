// Test-friendly Hyprland plugin shim.
// Exports PLUGIN_* symbols without linking compositor or including PluginAPI.hpp.
// Build with -DHYPR_AGENT_CURSORS_TEST_SHIM=1

#include <string>

#ifndef HYPRLAND_API_VERSION
#define HYPRLAND_API_VERSION "0.1"
#endif

#define APICALL extern "C"
#define EXPORT  __attribute__((visibility("default")))

using PLUGIN_DESCRIPTION_INFO = struct {
    std::string name;
    std::string description;
    std::string author;
    std::string version;
};

using HANDLE = void*;

// Named exactly as PLUGIN_*_FUNC_STR expects (pluginAPIVersion / pluginInit / pluginExit).
APICALL EXPORT std::string pluginAPIVersion() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO pluginInit(HANDLE /*handle*/) {
#if HYPR_AGENT_CURSORS_TEST_SHIM
    // Stub: no compositor, no HyprlandAPI calls.
    return PLUGIN_DESCRIPTION_INFO{
        .name        = "hypr-agent-cursors-shim",
        .description = "ABI test shim; does not touch compositor",
        .author      = "worker-abi",
        .version     = "0.0.0-test",
    };
#else
    return PLUGIN_DESCRIPTION_INFO{};
#endif
}

APICALL EXPORT void pluginExit() {
    // no-op
}

// Optional: client hash stub so nm/docs can mention hash ABI surface.
// Real plugins get __hyprland_api_get_client_hash from PluginAPI.hpp inline.
APICALL EXPORT const char* __hyprland_api_get_client_hash() {
    return "test-shim-no-hash";
}
