#include "plumas/platform/FileBackend.hpp"

#ifndef WIN32
#error "FileBackendWin32.cpp must only be built on Windows."
#endif

#include "plumas/core/FileIoTypes.hpp"

#include <optional>
#include <string>
#include <string_view>

#include <algorithm>
#include <windows.h>

namespace plumas::platform {

namespace {

// path.string() throws on Windows for names outside the ANSI code page, so
// validate the native wide string instead.
bool isValidPathString(const std::filesystem::path& path) {
    const std::wstring& native = path.native();
    return !native.empty() && native.size() <= core::kMaxPathLength
        && native.find(L'\0') == std::wstring::npos;
}

core::FileIoError errorFromWin32(const DWORD errorCode) {
    switch (errorCode) {
    case ERROR_ACCESS_DENIED:
        return core::FileIoError::PermissionDenied;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        return core::FileIoError::NotFound;
    default:
        return core::FileIoError::IoError;
    }
}

bool isReparsePoint(const std::filesystem::path& path) {
    const DWORD attributes = GetFileAttributesW(path.wstring().c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

std::optional<std::string> readLimitedHandle(const HANDLE handle, const std::size_t maxBytes) {
    LARGE_INTEGER fileSize {};
    if (!GetFileSizeEx(handle, &fileSize) || fileSize.QuadPart < 0
        || static_cast<unsigned long long>(fileSize.QuadPart) > maxBytes) {
        return std::nullopt;
    }

    // One allocation sized from the file; see the POSIX backend.
    std::string content;
    content.resize(static_cast<std::size_t>(fileSize.QuadPart));
    std::size_t used = 0;

    while (true) {
        if (used == content.size()) {
            if (content.size() >= maxBytes) {
                char probe = 0;
                DWORD extra = 0;
                if (ReadFile(handle, &probe, 1, &extra, nullptr) && extra == 0) {
                    return content;
                }
                return std::nullopt;
            }
            content.resize(std::min(maxBytes, std::max<std::size_t>(content.size() * 2, 4096)));
        }

        DWORD bytesRead = 0;
        const DWORD chunkSize = static_cast<DWORD>(
            std::min<std::size_t>(content.size() - used, static_cast<std::size_t>(MAXDWORD)));
        if (!ReadFile(handle, content.data() + used, chunkSize, &bytesRead, nullptr)) {
            return std::nullopt;
        }
        if (bytesRead == 0) {
            content.resize(used);
            return content;
        }
        used += static_cast<std::size_t>(bytesRead);
    }
}

} // namespace

core::FileReadResult readRegularTextFile(
    const std::filesystem::path& path,
    const std::size_t maxBytes) {
    core::FileReadResult result;

    if (!isValidPathString(path)) {
        result.error = core::FileIoError::InvalidPath;
        return result;
    }

    std::error_code error;
    if (std::filesystem::is_symlink(path, error)) {
        result.error = core::FileIoError::Symlink;
        return result;
    }

    if (isReparsePoint(path)) {
        result.error = core::FileIoError::Symlink;
        return result;
    }

    if (std::filesystem::is_directory(path, error)) {
        result.error = core::FileIoError::NotRegularFile;
        return result;
    }

    const HANDLE handle = CreateFileW(
        path.wstring().c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
        nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        result.error = errorFromWin32(GetLastError());
        return result;
    }

    std::optional<std::string> content = readLimitedHandle(handle, maxBytes);
    CloseHandle(handle);

    if (!content.has_value()) {
        result.error = core::FileIoError::TooLarge;
        return result;
    }

    result.content = std::move(*content);
    result.error = core::FileIoError::None;
    return result;
}

bool isSafeRegularFileSize(const std::filesystem::path& path, const std::size_t maxBytes) {
    if (!isValidPathString(path)) {
        return false;
    }

    std::error_code error;
    if (std::filesystem::is_symlink(path, error) || isReparsePoint(path)) {
        return false;
    }

    const HANDLE handle = CreateFileW(
        path.wstring().c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
        nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    LARGE_INTEGER fileSize {};
    const bool withinLimit = GetFileSizeEx(handle, &fileSize)
        && fileSize.QuadPart >= 0
        && static_cast<std::size_t>(fileSize.QuadPart) <= maxBytes;
    CloseHandle(handle);
    return withinLimit;
}

bool writeTextFileAtomic(
    const std::filesystem::path& path,
    const std::string_view content,
    const core::FilePermissions /*permissions*/) {
    // Windows ACLs are inherited from the parent directory, which is already
    // per-user for %APPDATA%; there is no POSIX mode to preserve.
    if (!isValidPathString(path)) {
        return false;
    }

    std::error_code error;
    if (std::filesystem::is_symlink(path, error) || isReparsePoint(path)) {
        return false;
    }

    const std::filesystem::path parent = path.parent_path();
    if (parent.empty()) {
        return false;
    }

    std::filesystem::create_directories(parent, error);

    const std::filesystem::path tempPath =
        parent / (path.filename().wstring() + L".pluma-text-tmp");

    const HANDLE handle = CreateFileW(
        tempPath.wstring().c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    const char* data = content.data();
    std::size_t totalWritten = 0;
    while (totalWritten < content.size()) {
        DWORD bytesWritten = 0;
        const DWORD chunkSize = static_cast<DWORD>(
            std::min<std::size_t>(content.size() - totalWritten, static_cast<std::size_t>(MAXDWORD)));
        if (!WriteFile(handle, data + totalWritten, chunkSize, &bytesWritten, nullptr)) {
            CloseHandle(handle);
            DeleteFileW(tempPath.wstring().c_str());
            return false;
        }
        totalWritten += static_cast<std::size_t>(bytesWritten);
    }

    FlushFileBuffers(handle);
    CloseHandle(handle);

    if (!MoveFileExW(
            tempPath.wstring().c_str(),
            path.wstring().c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tempPath.wstring().c_str());
        return false;
    }

    return true;
}

} // namespace plumas::platform
