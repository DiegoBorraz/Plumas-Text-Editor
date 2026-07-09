#pragma once

#include "plumas/core/FileIoTypes.hpp"

#include <atomic>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace plumas::core {

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
