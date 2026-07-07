#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace plumas::core {

constexpr std::size_t kMaxFileSizeBytes = 50U * 1024U * 1024U;
constexpr std::size_t kMaxEditorChars = kMaxFileSizeBytes / 4U;
constexpr std::size_t kMaxPathLength = 4096U;
constexpr std::size_t kMaxConfigJsonSize = 1024U * 1024U;

enum class FileIoError {
    None,
    InvalidPath,
    NotFound,
    NotRegularFile,
    Symlink,
    TooLarge,
    PermissionDenied,
    IoError,
};

struct FileReadResult {
    std::string content;
    FileIoError error = FileIoError::None;
};

bool isValidPathString(const std::string& path);
std::optional<std::filesystem::path> normalizePath(const std::string& path);
const std::vector<std::string>& allowedTextFileExtensionNames();
bool isAllowedTextFilePath(const std::filesystem::path& path);
bool isSafeRecentFilePath(const std::filesystem::path& path);
bool fileExistsForOverwrite(const std::filesystem::path& path);
FileReadResult readTextFile(const std::filesystem::path& path);
bool writeTextFileAtomic(const std::filesystem::path& path, const std::string& content);
bool restrictConfigPermissions(const std::filesystem::path& path);

} // namespace plumas::core
