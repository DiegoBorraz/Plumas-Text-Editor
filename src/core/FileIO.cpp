#include "plumas/core/FileIO.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <unordered_set>
#include <vector>

#include <algorithm>
#include <cctype>

namespace plumas::core {

namespace {

FileIoError errorFromErrno() {
    switch (errno) {
    case EACCES:
    case EPERM:
        return FileIoError::PermissionDenied;
    case ENOENT:
        return FileIoError::NotFound;
    case ELOOP:
        return FileIoError::Symlink;
    default:
        return FileIoError::IoError;
    }
}

bool isRegularFileDescriptor(int fd) {
    struct stat status {};
    if (::fstat(fd, &status) != 0) {
        return false;
    }
    return S_ISREG(status.st_mode);
}

std::optional<std::string> readLimited(int fd) {
    std::string content;
    content.reserve(4096);

    char buffer[8192];
    while (content.size() <= kMaxFileSizeBytes) {
        const ssize_t bytesRead = ::read(fd, buffer, sizeof(buffer));
        if (bytesRead < 0) {
            return std::nullopt;
        }
        if (bytesRead == 0) {
            return content;
        }

        const std::size_t newSize = content.size() + static_cast<std::size_t>(bytesRead);
        if (newSize > kMaxFileSizeBytes) {
            return std::nullopt;
        }

        content.append(buffer, static_cast<std::size_t>(bytesRead));
    }

    return std::nullopt;
}

} // namespace

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
    FileReadResult result;

    if (!isValidPathString(path.string())) {
        result.error = FileIoError::InvalidPath;
        return result;
    }

    std::error_code error;
    if (std::filesystem::is_symlink(path, error)) {
        result.error = FileIoError::Symlink;
        return result;
    }

    if (std::filesystem::is_directory(path, error)) {
        result.error = FileIoError::NotRegularFile;
        return result;
    }

    const int fd = ::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        result.error = errorFromErrno();
        return result;
    }

    if (!isRegularFileDescriptor(fd)) {
        ::close(fd);
        result.error = FileIoError::NotRegularFile;
        return result;
    }

    const std::optional<std::string> content = readLimited(fd);
    ::close(fd);

    if (!content.has_value()) {
        result.error = FileIoError::TooLarge;
        return result;
    }

    result.content = *content;
    result.error = FileIoError::None;
    return result;
}

bool writeTextFileAtomic(const std::filesystem::path& path, const std::string& content) {
    if (!isValidPathString(path.string())) {
        return false;
    }

    if (std::filesystem::is_symlink(path)) {
        return false;
    }

    const std::filesystem::path parent = path.parent_path();
    if (parent.empty()) {
        return false;
    }

    std::error_code error;
    std::filesystem::create_directories(parent, error);

    std::string tempTemplate = (parent / (path.filename().string() + ".plumas-tmp-XXXXXX")).string();
    std::vector<char> tempBuffer(tempTemplate.begin(), tempTemplate.end());
    tempBuffer.push_back('\0');

    const int fd = ::mkstemp(tempBuffer.data());
    if (fd < 0) {
        return false;
    }

    const std::string tempPath(tempBuffer.data());

    ssize_t totalWritten = 0;
    const char* data = content.data();
    const ssize_t totalSize = static_cast<ssize_t>(content.size());

    while (totalWritten < totalSize) {
        const ssize_t written = ::write(fd, data + totalWritten, totalSize - totalWritten);
        if (written < 0) {
            ::close(fd);
            ::unlink(tempPath.c_str());
            return false;
        }
        totalWritten += written;
    }

    if (::fsync(fd) != 0) {
        ::close(fd);
        ::unlink(tempPath.c_str());
        return false;
    }

    if (::close(fd) != 0) {
        ::unlink(tempPath.c_str());
        return false;
    }

    if (::rename(tempPath.c_str(), path.c_str()) != 0) {
        ::unlink(tempPath.c_str());
        return false;
    }

    return true;
}

bool restrictConfigPermissions(const std::filesystem::path& path) {
    return ::chmod(path.c_str(), S_IRUSR | S_IWUSR) == 0;
}

} // namespace plumas::core
