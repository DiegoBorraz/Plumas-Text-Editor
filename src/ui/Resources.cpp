#include "plumas/ui/Resources.hpp"

#include "plumas/core/FileIO.hpp"

#include <gio/gio.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace plumas::ui {

namespace {

constexpr const char kResourceBase[] = "/com/plumas/EditorTexto/";

bool fileExists(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::exists(path, error) && !error;
}

std::vector<std::filesystem::path> dataSearchDirectories() {
    std::vector<std::filesystem::path> directories;

#ifdef PLUMAS_INSTALL_DATADIR
    directories.emplace_back(
        std::filesystem::path(PLUMAS_INSTALL_DATADIR) / "plumas-editor-texto");
#endif

    if (const char* dataDirs = std::getenv("XDG_DATA_DIRS"); dataDirs != nullptr) {
        std::string_view remaining(dataDirs);
        while (!remaining.empty()) {
            const std::size_t separator = remaining.find(':');
            const std::string_view entry =
                separator == std::string_view::npos ? remaining : remaining.substr(0, separator);
            if (!entry.empty()) {
                directories.emplace_back(std::filesystem::path(entry) / "plumas-editor-texto");
            }
            if (separator == std::string_view::npos) {
                break;
            }
            remaining = remaining.substr(separator + 1);
        }
    }

    if (const char* home = std::getenv("HOME"); home != nullptr) {
        directories.emplace_back(
            std::filesystem::path(home) / ".local" / "share" / "plumas-editor-texto");
    }

    return directories;
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
    for (const std::filesystem::path& directory : dataSearchDirectories()) {
        const std::filesystem::path candidate = directory / fileName;
        if (fileExists(candidate)) {
            return candidate.string();
        }
    }

    std::error_code error;
    const std::filesystem::path executablePath =
        std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error) {
        const std::filesystem::path candidate =
            executablePath.parent_path() / "resources" / fileName;
        if (fileExists(candidate)) {
            return candidate.string();
        }
    }

    return std::nullopt;
}

} // namespace plumas::ui
