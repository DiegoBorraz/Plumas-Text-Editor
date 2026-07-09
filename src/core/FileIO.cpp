#include "plumas/core/FileIO.hpp"

#include "plumas/platform/FileBackend.hpp"

#include <unordered_set>

#include <algorithm>
#include <cctype>

namespace plumas::core {

bool isValidPathString(const std::string& path) {
    if (path.empty() || path.size() > kMaxPathLength) {
        return false;
    }
    if (path.find('\0') != std::string::npos) {
        return false;
    }
    return true;
}

std::optional<std::filesystem::path> normalizePath(const std::string& path) {
    if (!isValidPathString(path)) {
        return std::nullopt;
    }

    const std::filesystem::path input(path);
    std::error_code error;
    const std::filesystem::path normalized = std::filesystem::weakly_canonical(input, error);
    if (!error) {
        return normalized;
    }

    return input.lexically_normal();
}

const std::vector<std::string>& allowedTextFileExtensionNames() {
    static const std::vector<std::string> extensions = {
        "txt",
        "text",
        "log",
        "md",
        "markdown",
        "rst",
        "adoc",
        "tex",
        "json",
        "yaml",
        "yml",
        "toml",
        "xml",
        "csv",
        "tsv",
        "ini",
        "cfg",
        "conf",
        "properties",
        "html",
        "htm",
        "css",
        "scss",
        "sass",
        "less",
        "py",
        "pyw",
        "pyi",
        "java",
        "c",
        "h",
        "cpp",
        "cc",
        "cxx",
        "hpp",
        "hh",
        "hxx",
        "js",
        "jsx",
        "ts",
        "tsx",
        "mjs",
        "cjs",
        "go",
        "rs",
        "rb",
        "php",
        "swift",
        "kt",
        "kts",
        "scala",
        "cs",
        "lua",
        "pl",
        "r",
        "sql",
        "sh",
        "bash",
        "zsh",
        "fish",
        "env",
    };
    return extensions;
}

namespace {

const std::unordered_set<std::string>& allowedTextFileExtensionSet() {
    static const std::unordered_set<std::string> extensions(
        allowedTextFileExtensionNames().begin(),
        allowedTextFileExtensionNames().end());
    return extensions;
}

std::string extensionNameLowercase(const std::filesystem::path& path) {
    const std::string extension = path.extension().string();
    if (extension.size() < 2 || extension[0] != '.') {
        return {};
    }

    std::string name = extension.substr(1);
    std::transform(
        name.begin(),
        name.end(),
        name.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return name;
}

} // namespace

bool isAllowedTextFilePath(const std::filesystem::path& path) {
    const std::string extensionName = extensionNameLowercase(path);
    if (extensionName.empty()) {
        return false;
    }

    return allowedTextFileExtensionSet().contains(extensionName);
}

bool isSafeRecentFilePath(const std::filesystem::path& path) {
    if (!isValidPathString(path.string())) {
        return false;
    }

    if (!path.is_absolute()) {
        return false;
    }

    std::error_code error;
    if (std::filesystem::is_symlink(path, error)) {
        return false;
    }

    return true;
}

bool fileExistsForOverwrite(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::exists(path, error) && !error;
}

FileReadResult readTextFile(const std::filesystem::path& path) {
    return platform::readRegularTextFile(path, kMaxFileSizeBytes);
}

FileReadResult readSmallTextFile(const std::filesystem::path& path, const std::size_t maxBytes) {
    return platform::readRegularTextFile(path, maxBytes);
}

bool isSafeRegularFileSize(const std::filesystem::path& path, const std::size_t maxBytes) {
    return platform::isSafeRegularFileSize(path, maxBytes);
}

TextReplaceResult replaceAllInText(
    const std::string& text,
    const std::string& findText,
    const std::string& replaceText,
    const std::size_t maxReplacements,
    const std::size_t maxOutputBytes,
    const std::atomic<bool>* cancelFlag) {
    TextReplaceResult result;
    result.text = text;

    if (findText.empty()) {
        return result;
    }

    std::size_t position = 0;
    while (position <= result.text.size()) {
        if (cancelFlag != nullptr && cancelFlag->load()) {
            result.cancelled = true;
            return result;
        }

        if (result.replacements >= maxReplacements) {
            result.limitReached = true;
            return result;
        }

        const std::size_t matchPosition = result.text.find(findText, position);
        if (matchPosition == std::string::npos) {
            return result;
        }

        result.text.replace(matchPosition, findText.size(), replaceText);
        if (result.text.size() > maxOutputBytes) {
            result.limitReached = true;
            return result;
        }

        position = matchPosition + replaceText.size();
        ++result.replacements;
    }

    return result;
}

bool writeTextFileAtomic(const std::filesystem::path& path, const std::string& content) {
    return platform::writeTextFileAtomic(path, content);
}

bool restrictConfigPermissions(const std::filesystem::path& path) {
    return platform::restrictConfigPermissions(path);
}

} // namespace plumas::core
