// ABI symbol tests for Hyprland plugin .so — dlopen/dlsym only, no live load.
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// Match PluginAPI.hpp string literals exactly (do not include full headers here).
static constexpr const char* kApiVersionSym = "pluginAPIVersion";
static constexpr const char* kInitSym       = "pluginInit";
static constexpr const char* kExitSym       = "pluginExit";
static constexpr const char* kExpectedApi   = "0.1"; // HYPRLAND_API_VERSION

using PApiVersion = std::string (*)();
using PExit       = void (*)();

// Minimal mirror of PLUGIN_DESCRIPTION_INFO layout for shim call checks.
struct PluginDescriptionInfo {
    std::string name;
    std::string description;
    std::string author;
    std::string version;
};
using PInit = PluginDescriptionInfo (*)(void* handle);

static int g_failures = 0;

static void expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failures;
    } else {
        std::printf("PASS: %s\n", msg);
    }
}

int main(int argc, char** argv) {
    const char* so_path = argc > 1 ? argv[1] : "build/plugin_shim.so";

    void* handle = dlopen(so_path, RTLD_NOW);
    if (!handle) {
        std::fprintf(stderr, "FAIL: dlopen(%s): %s\n", so_path, dlerror());
        return 1;
    }
    expect(true, "dlopen RTLD_NOW");

    dlerror();
    void* p_ver = dlsym(handle, kApiVersionSym);
    const char* err = dlerror();
    expect(p_ver != nullptr && err == nullptr, "dlsym pluginAPIVersion");

    dlerror();
    void* p_init = dlsym(handle, kInitSym);
    err = dlerror();
    expect(p_init != nullptr && err == nullptr, "dlsym pluginInit");

    dlerror();
    void* p_exit = dlsym(handle, kExitSym);
    err = dlerror();
    expect(p_exit != nullptr && err == nullptr, "dlsym pluginExit");

    if (p_ver) {
        auto fn = reinterpret_cast<PApiVersion>(p_ver);
        std::string ver;
        try {
            ver = fn();
        } catch (...) {
            expect(false, "pluginAPIVersion threw");
            ver.clear();
        }
        expect(ver == kExpectedApi, "pluginAPIVersion returns HYPRLAND_API_VERSION (0.1)");
        if (ver != kExpectedApi)
            std::fprintf(stderr, "  got: '%s'\n", ver.c_str());
        else
            std::printf("  version string: %s\n", ver.c_str());
    }

    // Safe for test shim: stub init must not touch compositor.
    if (p_init) {
        auto fn = reinterpret_cast<PInit>(p_init);
        try {
            PluginDescriptionInfo info = fn(nullptr);
            expect(!info.name.empty(), "pluginInit returns non-empty name");
            std::printf("  init name=%s version=%s\n", info.name.c_str(), info.version.c_str());
        } catch (...) {
            expect(false, "pluginInit threw");
        }
    }

    if (p_exit) {
        auto fn = reinterpret_cast<PExit>(p_exit);
        try {
            fn();
            expect(true, "pluginExit callable");
        } catch (...) {
            expect(false, "pluginExit threw");
        }
    }

    dlclose(handle);

    if (g_failures) {
        std::fprintf(stderr, "\n%d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("\nAll ABI tests passed for %s\n", so_path);
    return 0;
}
