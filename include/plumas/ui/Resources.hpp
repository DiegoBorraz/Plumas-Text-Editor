#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace plumas::ui {

std::string resourcePath(const char* fileName);
bool hasBundledResource(const char* fileName);
bool isBundledResourceWithinSize(const char* fileName, std::size_t maxBytes);
bool isSafeFilesystemImagePath(const char* fileName, std::size_t maxBytes);
std::optional<std::string> findFilesystemResourcePath(const char* fileName);

} // namespace plumas::ui
