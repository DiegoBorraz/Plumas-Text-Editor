#include "plumas/platform/FileBackend.hpp"

#ifndef WIN32
#error "FileBackendWin32.cpp must only be built on Windows."
#endif

#include "plumas/core/FileIoTypes.hpp"

#include <optional>
#include <string>
#include <vector>

#include <algorithm>
#include <windows.h>

namespace plumas::platform {

namespace {

bool isValidPathString(const std::string& path) {
    if (path.empty() || path.size() > core::kMaxPathLength) {
        return false;
    }
    return path.find('\0') == std::string::npos;
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
    std::string content;
    content.reserve(4096);

    char buffer[8192];
    while (content.size() <= maxBytes) {
        DWORD bytesRead = 0;
        if (!ReadFile(handle, buffer, sizeof(buffer), &bytesRead, nullptr)) {
            return std::nullopt;
        }
        if (bytesRead == 0) {
            return content;
        }

        const std::size_t newSize = content.size() + static_cast<std::size_t>(bytesRead);
        if (newSize > maxBytes) {
            return std::nullopt;
        }

        content.append(buffer, static_cast<std::size_t>(bytesRead));
    }

    return std::nullopt;
}

} // namespace

core::FileReadResult readRegularTextFile(
    const std::filesystem::path& path,
    const std::size_t maxBytes) {
    core::FileReadResult result;

    if (!isValidPathString(path.string())) {
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

    const std::optional<std::string> content = readLimitedHandle(handle, maxBytes);
    CloseHandle(handle);

    if (!content.has_value()) {
        result.error = core::FileIoError::TooLarge;
        return result;
    }

    result.content = *content;
    result.error = core::FileIoError::None;
    return result;
}

bool isSafeRegularFileSize(const std::filesystem::path& path, const std::size_t maxBytes) {
    if (!isValidPathString(path.string())) {
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

bool writeTextFileAtomic(const std::filesystem::path& path, const std::string& content) {
    if (!isValidPathString(path.string())) {
        return false;
    }

    if (std::filesystem::is_symlink(path) || isReparsePoint(path)) {
        return false;
    }

    const std::filesystem::path parent = path.parent_path();
    if (parent.empty()) {
        return false;
    }

    std::error_code error;
    std::filesystem::create_directories(parent, error);

    const std::filesystem::path tempPath =
        parent / (path.filename().wstring() + L".plumas-tmp");

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

bool restrictConfigPermissions(const std::filesystem::path& path) {
    return SetFileAttributesW(path.wstring().c_str(), FILE_ATTRIBUTE_NORMAL) != 0;
}

} // namespace plumas::platform
