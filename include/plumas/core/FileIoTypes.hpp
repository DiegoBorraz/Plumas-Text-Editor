#pragma once

#include <cstddef>
#include <string>

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

} // namespace plumas::core
