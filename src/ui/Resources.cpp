#include "plumas/ui/Resources.hpp"

#include "plumas/core/FileIO.hpp"
#include "plumas/platform/Paths.hpp"

#include <gio/gio.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace plumas::ui {

namespace {

constexpr const char kResourceBase[] = "/com/plumas/EditorTexto/";

bool fileExists(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::exists(path, error) && !error;
}

} // namespace

std::string resourcePath(const char* fileName) {
    return std::string(kResourceBase) + fileName;
}

bool hasBundledResource(const char* fileName) {
    return isBundledResourceWithinSize(fileName, core::kMaxIconFileSizeBytes);
}

bool isBundledResourceWithinSize(const char* fileName, const std::size_t maxBytes) {
    g_autoptr(GBytes) data = g_resources_lookup_data(
        resourcePath(fileName).c_str(),
        G_RESOURCE_LOOKUP_FLAGS_NONE,
        nullptr);
    return data != nullptr && g_bytes_get_size(data) <= maxBytes;
}

bool isSafeFilesystemImagePath(const char* fileName, const std::size_t maxBytes) {
    const std::optional<std::string> path = findFilesystemResourcePath(fileName);
    if (!path.has_value()) {
        return false;
    }

    return core::isSafeRegularFileSize(*path, maxBytes);
}

std::optional<std::string> findFilesystemResourcePath(const char* fileName) {
    for (const std::filesystem::path& directory : platform::dataDirectories()) {
        const std::filesystem::path candidate = directory / fileName;
        if (fileExists(candidate)) {
            return candidate.string();
        }
    }

    const std::filesystem::path candidate =
        platform::executableDirectory() / "resources" / fileName;
    if (fileExists(candidate)) {
        return candidate.string();
    }

    return std::nullopt;
}

} // namespace plumas::ui
