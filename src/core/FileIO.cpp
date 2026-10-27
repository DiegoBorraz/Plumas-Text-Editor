#include "plumas/core/FileIO.hpp"

#include "plumas/platform/FileBackend.hpp"

#include <algorithm>
#include <cctype>

namespace plumas::core {

std::filesystem::path pathFromUtf8(const std::string_view utf8) {
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

std::string pathToUtf8(const std::filesystem::path& path) {
    const std::u8string utf8 = path.u8string();
    return std::string(utf8.begin(), utf8.end());
}

std::optional<std::filesystem::path> normalizePath(const std::string& path) {
    if (!isValidPathString(path)) {
        return std::nullopt;
    }

    // Symlinks are resolved on purpose: editing a link edits its target, as in
    // other editors, and the link itself survives the atomic save. The backend
    // still refuses a final-component symlink, so a link swapped in after this
    // point makes the read or write fail instead of being followed.
    const std::filesystem::path input = pathFromUtf8(path);
    std::error_code error;
    std::filesystem::path normalized = std::filesystem::weakly_canonical(input, error);
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

std::string extensionNameLowercase(const std::filesystem::path& path) {
    const std::string extension = pathToUtf8(path.extension());
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

    const std::vector<std::string>& allowed = allowedTextFileExtensionNames();
    return std::find(allowed.begin(), allowed.end(), extensionName) != allowed.end();
}

bool isValidUtf8Text(const std::string_view text) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(text.data());
    const std::size_t size = text.size();
    std::size_t index = 0;

    while (index < size) {
        const unsigned char lead = bytes[index];
        if (lead == 0) {
            return false;
        }
        if (lead < 0x80) {
            ++index;
            continue;
        }

        std::size_t length = 0;
        unsigned char minSecond = 0x80;
        unsigned char maxSecond = 0xBF;
        if (lead >= 0xC2 && lead <= 0xDF) {
            length = 2;
        } else if (lead >= 0xE0 && lead <= 0xEF) {
            length = 3;
            if (lead == 0xE0) {
                minSecond = 0xA0; // overlong
            } else if (lead == 0xED) {
                maxSecond = 0x9F; // UTF-16 surrogates
            }
        } else if (lead >= 0xF0 && lead <= 0xF4) {
            length = 4;
            if (lead == 0xF0) {
                minSecond = 0x90; // overlong
            } else if (lead == 0xF4) {
                maxSecond = 0x8F; // above U+10FFFF
            }
        } else {
            return false;
        }

        if (size - index < length || bytes[index + 1] < minSecond || bytes[index + 1] > maxSecond) {
            return false;
        }
        for (std::size_t offset = 2; offset < length; ++offset) {
            if ((bytes[index + offset] & 0xC0) != 0x80) {
                return false;
            }
        }
        index += length;
    }
    return true;
}

FileReadResult readTextFile(const std::filesystem::path& path, const std::size_t maxBytes) {
    return platform::readRegularTextFile(path, maxBytes);
}

bool isSafeRegularFileSize(const std::filesystem::path& path, const std::size_t maxBytes) {
    return platform::isSafeRegularFileSize(path, maxBytes);
}

TextReplaceResult replaceAllInText(
    const std::string_view text,
    const std::string_view findText,
    const std::string_view replaceText,
    const std::size_t maxReplacements,
    const std::size_t maxOutputBytes,
    const std::atomic<bool>* cancelFlag) {
    TextReplaceResult result;
    if (findText.empty()) {
        return result;
    }

    // Builds the output in one pass. Replacing in place would shift the tail of
    // the string on every match, which is quadratic on large documents.
    std::size_t position = 0;
    while (true) {
        if (cancelFlag != nullptr && cancelFlag->load(std::memory_order_relaxed)) {
            result = {};
            result.cancelled = true;
            return result;
        }

        const std::size_t matchPosition = text.find(findText, position);
        if (matchPosition == std::string_view::npos) {
            break;
        }

        if (result.replacements >= maxReplacements) {
            result = {};
            result.limitReached = true;
            return result;
        }

        if (result.replacements == 0) {
            result.text.reserve(text.size());
        }
        result.text.append(text.substr(position, matchPosition - position));
        result.text.append(replaceText);
        if (result.text.size() > maxOutputBytes) {
            result = {};
            result.limitReached = true;
            return result;
        }

        position = matchPosition + findText.size();
        ++result.replacements;
    }

    if (result.replacements == 0) {
        return result;
    }

    result.text.append(text.substr(position));
    if (result.text.size() > maxOutputBytes) {
        result = {};
        result.limitReached = true;
    }
    return result;
}

bool writeTextFileAtomic(
    const std::filesystem::path& path,
    const std::string_view content,
    const FilePermissions permissions) {
    return platform::writeTextFileAtomic(path, content, permissions);
}

} // namespace plumas::core
