#include "plumas/core/Document.hpp"
#include "plumas/core/FileIO.hpp"

namespace plumas::core {

const std::string& Document::content() const {
    return content_;
}

const std::string& Document::filePath() const {
    return filePath_;
}

bool Document::isDirty() const {
    return isDirty_;
}

void Document::setContent(const std::string& newContent) {
    if (content_ != newContent) {
        content_ = newContent;
        isDirty_ = true;
    }
}

void Document::markDirty() {
    isDirty_ = true;
}

bool Document::load(const std::string& path) {
    const std::optional<std::filesystem::path> normalized = normalizePath(path);
    if (!normalized.has_value() || !isAllowedTextFilePath(*normalized)) {
        return false;
    }

    const FileReadResult result = readTextFile(*normalized);
    if (result.error != FileIoError::None) {
        return false;
    }

    content_ = result.content;
    filePath_ = normalized->string();
    isDirty_ = false;
    return true;
}

bool Document::save() {
    if (filePath_.empty()) {
        return false;
    }
    return saveAs(filePath_);
}

bool Document::saveAs(const std::string& path) {
    const std::optional<std::filesystem::path> normalized = normalizePath(path);
    if (!normalized.has_value()) {
        return false;
    }

    if (std::filesystem::is_symlink(*normalized)) {
        return false;
    }

    if (!isAllowedTextFilePath(*normalized)) {
        return false;
    }

    if (!writeTextFileAtomic(*normalized, content_)) {
        return false;
    }

    filePath_ = normalized->string();
    isDirty_ = false;
    return true;
}

} // namespace plumas::core
