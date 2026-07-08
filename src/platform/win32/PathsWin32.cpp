#include "plumas/platform/Paths.hpp"

#ifndef WIN32
#error "PathsWin32.cpp must only be built on Windows."
#endif

#include "plumas/core/FileIoTypes.hpp"

#include <filesystem>
#include <string>
#include <vector>

#include <shlobj.h>
#include <windows.h>

namespace plumas::platform {

namespace {

constexpr const char kAppConfigDirName[] = "plumas-text-editor";

} // namespace

std::filesystem::path configDirectoryWin32() {
    wchar_t appDataPath[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appDataPath))) {
        return std::filesystem::path(appDataPath) / kAppConfigDirName;
    }
    return std::filesystem::path(kAppConfigDirName);
}

std::filesystem::path executableDirectoryWin32() {
    wchar_t modulePath[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(modulePath).parent_path();
}

std::vector<std::filesystem::path> dataDirectoriesWin32() {
    std::vector<std::filesystem::path> directories;

#ifdef PLUMAS_INSTALL_DATADIR
    directories.emplace_back(
        std::filesystem::path(PLUMAS_INSTALL_DATADIR) / kAppConfigDirName);
#endif

    const std::filesystem::path executableDirectory = executableDirectoryWin32();
    directories.emplace_back(executableDirectory / "share" / kAppConfigDirName);

    wchar_t appDataPath[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appDataPath))) {
        directories.emplace_back(std::filesystem::path(appDataPath) / kAppConfigDirName);
    }

    return directories;
}

} // namespace plumas::platform
