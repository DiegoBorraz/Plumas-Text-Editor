#include "plumas/platform/Paths.hpp"

namespace plumas::platform {

#if defined(_WIN32)
std::filesystem::path configDirectoryWin32();
std::filesystem::path executableDirectoryWin32();
std::vector<std::filesystem::path> dataDirectoriesWin32();
#else
std::filesystem::path configDirectoryPosix();
std::filesystem::path executableDirectoryPosix();
std::vector<std::filesystem::path> dataDirectoriesPosix();
#endif

std::filesystem::path configDirectory() {
#if defined(_WIN32)
    return configDirectoryWin32();
#else
    return configDirectoryPosix();
#endif
}

std::filesystem::path executableDirectory() {
#if defined(_WIN32)
    return executableDirectoryWin32();
#else
    return executableDirectoryPosix();
#endif
}

std::vector<std::filesystem::path> dataDirectories() {
#if defined(_WIN32)
    return dataDirectoriesWin32();
#else
    return dataDirectoriesPosix();
#endif
}

bool usesNativeWindowChrome() {
    // Linux and Windows draw the Win11-style title row themselves (GTK4's
    // usual client-side decorations, with Aero Snap emulated on Windows).
    // macOS keeps its native title bar and traffic-light buttons.
#if defined(__APPLE__)
    return true;
#else
    return false;
#endif
}

bool usesCrlfByDefault() {
#if defined(_WIN32)
    return true;
#else
    return false;
#endif
}

} // namespace plumas::platform
