#include "plumas/core/Config.hpp"
#include "plumas/core/FileIO.hpp"

#include "plumas/platform/Paths.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>

namespace plumas::core {

namespace {

constexpr std::size_t kMaxRecentFiles = 10;

std::optional<std::string> normalizeRecentPath(const std::string& path) {
    const std::optional<std::filesystem::path> normalized = normalizePath(path);
    if (!normalized.has_value() || !normalized->is_absolute()
        || !isAllowedTextFilePath(*normalized)) {
        return std::nullopt;
    }
    // Linux file names are raw bytes; JSON (and the recent-files menu) need UTF-8.
    std::string utf8 = pathToUtf8(*normalized);
    if (!isValidUtf8Text(utf8)) {
        return std::nullopt;
    }
    return utf8;
}

} // namespace

Config::Config() : path_(defaultPath()) {}

std::filesystem::path Config::defaultPath() {
    return platform::configDirectory() / "config.json";
}

std::filesystem::path Config::legacyPath() {
    // Releases named "Plumas" kept their config in a sibling directory.
    return platform::configDirectory().parent_path() / "plumas-text-editor" / "config.json";
}

bool Config::load() {
    std::error_code error;
    std::filesystem::path source = path_;
    if (!std::filesystem::exists(source, error) && path_ == defaultPath()) {
        // The next save writes the new location; the old file is left alone.
        source = legacyPath();
    }
    if (!std::filesystem::exists(source, error)) {
        recentFiles_.clear();
        return true;
    }

    const FileReadResult fileContent = readTextFile(source, kMaxConfigJsonSize);
    if (fileContent.error != FileIoError::None) {
        recentFiles_.clear();
        return false;
    }

    try {
        const nlohmann::json data = nlohmann::json::parse(fileContent.content);
        recentFiles_.clear();

        if (data.contains("recentFiles") && data["recentFiles"].is_array()) {
            for (const auto& entry : data["recentFiles"]) {
                if (!entry.is_string()) {
                    continue;
                }

                std::optional<std::string> normalized =
                    normalizeRecentPath(entry.get<std::string>());
                if (normalized.has_value() && recentFiles_.size() < kMaxRecentFiles) {
                    recentFiles_.push_back(std::move(*normalized));
                }
            }
        }
    } catch (const nlohmann::json::exception&) {
        recentFiles_.clear();
        return false;
    }

    return true;
}

bool Config::save() const {
    std::error_code error;
    std::filesystem::create_directories(path_.parent_path(), error);

    nlohmann::json data;
    data["recentFiles"] = recentFiles_;

    // Entries are validated on insert; replacing instead of throwing keeps an
    // exception from unwinding through the GTK callbacks that call save().
    const std::string text = data.dump(2, ' ', false, nlohmann::json::error_handler_t::replace);
    return writeTextFileAtomic(path_, text, FilePermissions::OwnerOnly);
}

const std::vector<std::string>& Config::recentFiles() const {
    return recentFiles_;
}

void Config::addRecentFile(const std::string& path) {
    if (path.empty()) {
        return;
    }

    const std::optional<std::string> normalized = normalizeRecentPath(path);
    if (!normalized.has_value()) {
        return;
    }

    const std::string& normalizedPath = *normalized;
    auto it = std::find(recentFiles_.begin(), recentFiles_.end(), normalizedPath);
    if (it != recentFiles_.end()) {
        recentFiles_.erase(it);
    }

    recentFiles_.insert(recentFiles_.begin(), normalizedPath);

    if (recentFiles_.size() > kMaxRecentFiles) {
        recentFiles_.resize(kMaxRecentFiles);
    }
}

} // namespace plumas::core
