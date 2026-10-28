#pragma once

#include "plumas/core/FileIoTypes.hpp"

#include <filesystem>
#include <string_view>

namespace plumas::platform {

core::FileReadResult readRegularTextFile(
    const std::filesystem::path& path,
    std::size_t maxBytes);
bool isSafeRegularFileSize(const std::filesystem::path& path, std::size_t maxBytes);
bool writeTextFileAtomic(
    const std::filesystem::path& path,
    std::string_view content,
    core::FilePermissions permissions);

} // namespace plumas::platform
