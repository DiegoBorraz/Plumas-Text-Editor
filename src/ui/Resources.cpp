#include "plumas/ui/Resources.hpp"

#include <filesystem>

namespace plumas::ui {

std::filesystem::path getExecutableDir() {
    std::error_code error;
    const std::filesystem::path exePath = std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error) {
        return exePath.parent_path();
    }
    return std::filesystem::current_path();
}

std::optional<std::filesystem::path> findResourcePath(const char* fileName) {
    const std::filesystem::path candidate = getExecutableDir() / "resources" / fileName;

    std::error_code error;
    if (std::filesystem::exists(candidate, error) && !error) {
        return candidate;
    }

    return std::nullopt;
}

} // namespace plumas::ui
