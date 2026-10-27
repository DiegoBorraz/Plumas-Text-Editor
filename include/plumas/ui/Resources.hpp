#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace plumas::ui {

std::string resourcePath(const char* fileName);
bool hasBundledResource(const char* fileName);
bool isBundledResourceWithinSize(const char* fileName, std::size_t maxBytes);
// Returns the path that passed the checks, so callers load that same file.
std::optional<std::string> findSafeFilesystemImagePath(const char* fileName, std::size_t maxBytes);
std::optional<std::string> findFilesystemResourcePath(const char* fileName);

} // namespace plumas::ui
