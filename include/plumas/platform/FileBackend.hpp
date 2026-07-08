#pragma once

#include "plumas/core/FileIoTypes.hpp"

#include <filesystem>
#include <string>

namespace plumas::platform {

core::FileReadResult readRegularTextFile(
    const std::filesystem::path& path,
    std::size_t maxBytes);
bool isSafeRegularFileSize(const std::filesystem::path& path, std::size_t maxBytes);
bool writeTextFileAtomic(const std::filesystem::path& path, const std::string& content);
bool restrictConfigPermissions(const std::filesystem::path& path);

} // namespace plumas::platform
