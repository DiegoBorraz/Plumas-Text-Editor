#pragma once

#include <optional>
#include <string>

namespace plumas::ui {

std::string resourcePath(const char* fileName);
bool hasBundledResource(const char* fileName);
std::optional<std::string> findFilesystemResourcePath(const char* fileName);

} // namespace plumas::ui
