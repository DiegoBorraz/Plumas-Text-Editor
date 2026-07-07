#pragma once

#include <string>

namespace plumas::core {

class Document {
public:
    Document() = default;

    const std::string& content() const;
    const std::string& filePath() const;
    bool isDirty() const;

    void setContent(const std::string& newContent);
    void markDirty();

    bool load(const std::string& path);
    bool save();
    bool saveAs(const std::string& path);

private:
    std::string content_;
    std::string filePath_;
    bool isDirty_ = false;
};

} // namespace plumas::core
