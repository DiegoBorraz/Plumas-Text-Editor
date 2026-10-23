#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace plumas::core {

// Tracks the file behind the editor. The text itself lives only in the editor
// buffer, so the document never holds a second copy of it.
class Document {
public:
    Document() = default;

    const std::string& filePath() const;
    bool isDirty() const;

    void markDirty();

    // Reads the file and adopts its path. The returned text is the only copy;
    // the caller hands it to the editor and drops it.
    std::optional<std::string> load(const std::string& path);
    bool saveAs(const std::string& path, std::string_view content);

private:
    std::string filePath_;
    bool isDirty_ = false;
};

} // namespace plumas::core
