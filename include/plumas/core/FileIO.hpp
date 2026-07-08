#pragma once

#include <atomic>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace plumas::core {

constexpr std::size_t kMaxFileSizeBytes = 50U * 1024U * 1024U;
constexpr std::size_t kMaxEditorChars = kMaxFileSizeBytes / 4U;
constexpr std::size_t kMaxPathLength = 4096U;
constexpr std::size_t kMaxConfigJsonSize = 1024U * 1024U;
constexpr std::size_t kMaxIconFileSizeBytes = 512U * 1024U;
constexpr std::size_t kMaxReplaceAllReplacements = 1'000'000U;

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

struct TextReplaceResult {
    std::string text;
    std::size_t replacements = 0;
    bool cancelled = false;
    bool limitReached = false;
};

bool isValidPathString(const std::string& path);
std::optional<std::filesystem::path> normalizePath(const std::string& path);
const std::vector<std::string>& allowedTextFileExtensionNames();
bool isAllowedTextFilePath(const std::filesystem::path& path);
bool isSafeRecentFilePath(const std::filesystem::path& path);
bool fileExistsForOverwrite(const std::filesystem::path& path);
FileReadResult readTextFile(const std::filesystem::path& path);
FileReadResult readSmallTextFile(const std::filesystem::path& path, std::size_t maxBytes);
bool isSafeRegularFileSize(const std::filesystem::path& path, std::size_t maxBytes);
TextReplaceResult replaceAllInText(
    const std::string& text,
    const std::string& findText,
    const std::string& replaceText,
    std::size_t maxReplacements,
    std::size_t maxOutputBytes,
    const std::atomic<bool>* cancelFlag);
bool writeTextFileAtomic(const std::filesystem::path& path, const std::string& content);
bool restrictConfigPermissions(const std::filesystem::path& path);

} // namespace plumas::core
