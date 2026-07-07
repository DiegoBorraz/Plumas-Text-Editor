#pragma once

#include <filesystem>
#include <optional>

namespace plumas::ui {

std::filesystem::path getExecutableDir();
std::optional<std::filesystem::path> findResourcePath(const char* fileName);

} // namespace plumas::ui
