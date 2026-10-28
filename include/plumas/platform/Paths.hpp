#pragma once

#include <filesystem>
#include <vector>

namespace plumas::platform {

std::filesystem::path configDirectory();
std::filesystem::path executableDirectory();
std::vector<std::filesystem::path> dataDirectories();
bool usesNativeWindowChrome();
// Line ending shown for new documents.
bool usesCrlfByDefault();

} // namespace plumas::platform
