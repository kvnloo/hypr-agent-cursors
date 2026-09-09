// Real plugin skeleton for hypr-agent-cursors.
// Includes PluginAPI.hpp. Builds a .so that exports pluginAPIVersion/pluginInit/pluginExit.
// Offline dlopen(RTLD_NOW) fails: undefined hyprland-internal symbols (resolved only inside compositor).
// NEVER hyprctl plugin load from this workdir — ABI/build verification only.

#include <plugins/PluginAPI.hpp>

#include <string>

// Plugin handle retained for HyprlandAPI calls when loaded live.
HANDLE PHANDLE = nullptr;

/*
 * Hash check (client vs server) per PluginAPI.hpp:
 *   __hyprland_api_get_hash()        — provided by Hyprland binary (server)
 *   __hyprland_api_get_client_hash() — inline in header, compiled into this .so
 * On mismatch Hyprland ejects the plugin before PLUGIN_INIT.
 */

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    // Live path: HyprlandAPI::* registration goes here.
    // Offline/test builds must not call compositor APIs.

    return PLUGIN_DESCRIPTION_INFO{
        .name        = "hypr-agent-cursors",
        .description = "Independent agent cursor surfaces (skeleton)",
        .author      = "worker-abi",
        .version     = "0.1.0",
    };
}

APICALL EXPORT void PLUGIN_EXIT() {
    PHANDLE = nullptr;
}
