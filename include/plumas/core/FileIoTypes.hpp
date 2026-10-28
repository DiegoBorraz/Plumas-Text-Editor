#pragma once

#include <cstddef>
#include <string>
#include <string_view>

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

enum class FilePermissions {
    // Keep the mode of the file being replaced; new files follow the umask.
    PreserveOrDefault,
    // Readable and writable by the owner only (config files).
    OwnerOnly,
};

inline bool isValidPathString(const std::string_view path) {
    return !path.empty() && path.size() <= kMaxPathLength
        && path.find('\0') == std::string_view::npos;
}

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

} // namespace plumas::core
