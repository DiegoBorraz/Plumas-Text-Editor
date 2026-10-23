#include "plumas/core/Document.hpp"
#include "plumas/core/FileIO.hpp"

namespace plumas::core {

const std::string& Document::filePath() const {
    return filePath_;
}

bool Document::isDirty() const {
    return isDirty_;
}

void Document::markDirty() {
    isDirty_ = true;
}

std::optional<std::string> Document::load(const std::string& path) {
    const std::optional<std::filesystem::path> normalized = normalizePath(path);
    if (!normalized.has_value() || !isAllowedTextFilePath(*normalized)) {
        return std::nullopt;
    }

    FileReadResult result = readTextFile(*normalized, kMaxFileSizeBytes);
    if (result.error != FileIoError::None) {
        return std::nullopt;
    }

    filePath_ = pathToUtf8(*normalized);
    isDirty_ = false;
    return std::move(result.content);
}

bool Document::saveAs(const std::string& path, const std::string_view content) {
    if (content.size() > kMaxFileSizeBytes) {
        return false;
    }

    const std::optional<std::filesystem::path> normalized = normalizePath(path);
    if (!normalized.has_value() || !isAllowedTextFilePath(*normalized)) {
        return false;
    }

    if (!writeTextFileAtomic(*normalized, content, FilePermissions::PreserveOrDefault)) {
        return false;
    }

    filePath_ = pathToUtf8(*normalized);
    isDirty_ = false;
    return true;
}

} // namespace plumas::core
