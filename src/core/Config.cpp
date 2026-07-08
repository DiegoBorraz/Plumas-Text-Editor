#include "plumas/core/Config.hpp"
#include "plumas/core/FileIO.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>

namespace plumas::core {

namespace {

constexpr std::size_t kMaxRecentFiles = 10;

} // namespace

Config::Config() : path_(defaultPath()) {}

std::filesystem::path Config::defaultPath() {
    if (const char* xdgConfigHome = std::getenv("XDG_CONFIG_HOME");
        xdgConfigHome != nullptr && isValidPathString(xdgConfigHome)) {
        return std::filesystem::path(xdgConfigHome) / "plumas-editor-texto" / "config.json";
    }

    const char* home = std::getenv("HOME");
    if (home != nullptr && isValidPathString(home)) {
        return std::filesystem::path(home) / ".config" / "plumas-editor-texto" / "config.json";
    }

    return std::filesystem::path(".config") / "plumas-editor-texto" / "config.json";
}

bool Config::load() {
    std::error_code error;
    if (!std::filesystem::exists(path_, error)) {
        recentFiles_.clear();
        return true;
    }

    const FileReadResult fileContent = readSmallTextFile(path_, kMaxConfigJsonSize);
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

                const std::optional<std::filesystem::path> normalized =
                    normalizePath(entry.get<std::string>());
                if (!normalized.has_value() || !isSafeRecentFilePath(*normalized)) {
                    continue;
                }

                recentFiles_.push_back(normalized->string());
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

    std::ofstream file(path_);
    if (!file.is_open()) {
        return false;
    }

    file << data.dump(2);
    file.close();

    restrictConfigPermissions(path_);
    return true;
}

const std::vector<std::string>& Config::recentFiles() const {
    return recentFiles_;
}

void Config::addRecentFile(const std::string& path) {
    if (path.empty()) {
        return;
    }

    const std::optional<std::filesystem::path> normalized = normalizePath(path);
    if (!normalized.has_value() || !isSafeRecentFilePath(*normalized)
        || !isAllowedTextFilePath(*normalized)) {
        return;
    }

    const std::string normalizedPath = normalized->string();
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
