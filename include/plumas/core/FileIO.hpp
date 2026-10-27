#pragma once

#include "plumas/core/FileIoTypes.hpp"

#include <atomic>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace plumas::core {

// GTK hands out UTF-8 paths on every platform, but std::filesystem::path's
// std::string overloads use the ANSI code page on Windows. Always convert
// through these two helpers.
std::filesystem::path pathFromUtf8(std::string_view utf8);
std::string pathToUtf8(const std::filesystem::path& path);

std::optional<std::filesystem::path> normalizePath(const std::string& path);
const std::vector<std::string>& allowedTextFileExtensionNames();
bool isAllowedTextFilePath(const std::filesystem::path& path);
// True for UTF-8 without NUL bytes, the only text GtkTextBuffer accepts.
bool isValidUtf8Text(std::string_view text);
// Decided by the first line break, as Notepad does.
bool usesCrlfLineEndings(std::string_view text);
FileReadResult readTextFile(const std::filesystem::path& path, std::size_t maxBytes);
bool isSafeRegularFileSize(const std::filesystem::path& path, std::size_t maxBytes);
TextReplaceResult replaceAllInText(
    std::string_view text,
    std::string_view findText,
    std::string_view replaceText,
    std::size_t maxReplacements,
    std::size_t maxOutputBytes,
    const std::atomic<bool>* cancelFlag);
bool writeTextFileAtomic(
    const std::filesystem::path& path,
    std::string_view content,
    FilePermissions permissions);

} // namespace plumas::core
