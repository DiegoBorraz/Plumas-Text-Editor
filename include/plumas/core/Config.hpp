#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace plumas::core {

class Config {
public:
    Config();

    static std::filesystem::path defaultPath();

    bool load();
    bool save() const;

    const std::vector<std::string>& recentFiles() const;
    void addRecentFile(const std::string& path);

private:
    std::filesystem::path path_;
    std::vector<std::string> recentFiles_;
};

} // namespace plumas::core
