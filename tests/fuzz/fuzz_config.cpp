// libFuzzer target: arbitrary bytes as config.json, then a save round trip.
// Build: see tests/fuzz/README.md.

#include "plumas/core/Config.hpp"
#include "plumas/core/FileIO.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <unistd.h>

namespace {

namespace fs = std::filesystem;

// One private directory per fuzzing process, used as XDG_CONFIG_HOME.
const fs::path& configHome() {
    static const fs::path home = [] {
        fs::path dir = fs::temp_directory_path() / ("pluma-fuzz-" + std::to_string(::getpid()));
        fs::create_directories(dir);
        ::setenv("XDG_CONFIG_HOME", dir.c_str(), 1);
        return dir;
    }();
    return home;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    configHome();
    const fs::path path = plumas::core::Config::defaultPath();
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary | std::ios::trunc)
        .write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));

    // load() must reject bad input by returning false, never by throwing.
    plumas::core::Config config;
    config.load();
    for (const std::string& entry : config.recentFiles()) {
        if (!plumas::core::isValidUtf8Text(entry)) {
            std::abort();
        }
    }

    // Whatever load() kept must survive a save and reload unchanged.
    if (config.save()) {
        plumas::core::Config reloaded;
        if (!reloaded.load() || reloaded.recentFiles() != config.recentFiles()) {
            std::abort();
        }
    }
    return 0;
}
