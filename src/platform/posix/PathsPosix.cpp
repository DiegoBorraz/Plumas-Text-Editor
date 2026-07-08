#include "plumas/platform/Paths.hpp"

#include "plumas/core/FileIoTypes.hpp"

#include <cstdlib>
#include <filesystem>
#include <string_view>
#include <vector>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace plumas::platform {

namespace {

constexpr const char kAppConfigDirName[] = "plumas-text-editor";

bool isValidEnvPath(const char* value) {
    if (value == nullptr) {
        return false;
    }
    const std::string_view path(value);
    return !path.empty() && path.size() <= core::kMaxPathLength && path.find('\0') == std::string_view::npos;
}

} // namespace

std::filesystem::path configDirectoryPosix() {
#if defined(__APPLE__)
    if (const char* home = std::getenv("HOME"); home != nullptr && isValidEnvPath(home)) {
        return std::filesystem::path(home) / "Library" / "Application Support" / kAppConfigDirName;
    }
    return std::filesystem::path("Library") / "Application Support" / kAppConfigDirName;
#else
    if (const char* xdgConfigHome = std::getenv("XDG_CONFIG_HOME");
        xdgConfigHome != nullptr && isValidEnvPath(xdgConfigHome)) {
        return std::filesystem::path(xdgConfigHome) / kAppConfigDirName;
    }

    if (const char* home = std::getenv("HOME"); home != nullptr && isValidEnvPath(home)) {
        return std::filesystem::path(home) / ".config" / kAppConfigDirName;
    }

    return std::filesystem::path(".config") / kAppConfigDirName;
#endif
}

std::filesystem::path executableDirectoryPosix() {
#if defined(__APPLE__)
    std::vector<char> buffer(4096);
    uint32_t size = static_cast<uint32_t>(buffer.size());
    if (_NSGetExecutablePath(buffer.data(), &size) == 0) {
        return std::filesystem::path(buffer.data()).parent_path();
    }
    return std::filesystem::current_path();
#else
    std::error_code error;
    const std::filesystem::path executablePath =
        std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error) {
        return executablePath.parent_path();
    }
    return std::filesystem::current_path();
#endif
}

std::vector<std::filesystem::path> dataDirectoriesPosix() {
    std::vector<std::filesystem::path> directories;

#ifdef PLUMAS_INSTALL_DATADIR
    directories.emplace_back(
        std::filesystem::path(PLUMAS_INSTALL_DATADIR) / kAppConfigDirName);
#endif

#if !defined(__APPLE__)
    if (const char* dataDirs = std::getenv("XDG_DATA_DIRS"); dataDirs != nullptr) {
        std::string_view remaining(dataDirs);
        while (!remaining.empty()) {
            const std::size_t separator = remaining.find(':');
            const std::string_view entry =
                separator == std::string_view::npos ? remaining : remaining.substr(0, separator);
            if (!entry.empty()) {
                directories.emplace_back(std::filesystem::path(entry) / kAppConfigDirName);
            }
            if (separator == std::string_view::npos) {
                break;
            }
            remaining = remaining.substr(separator + 1);
        }
    }
#endif

    if (const char* home = std::getenv("HOME"); home != nullptr) {
#if defined(__APPLE__)
        directories.emplace_back(
            std::filesystem::path(home) / "Library" / "Application Support" / kAppConfigDirName);
#else
        directories.emplace_back(
            std::filesystem::path(home) / ".local" / "share" / kAppConfigDirName);
#endif
    }

    return directories;
}

} // namespace plumas::platform
