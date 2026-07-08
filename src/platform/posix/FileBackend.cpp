#include "plumas/platform/FileBackend.hpp"

#include "plumas/core/FileIoTypes.hpp"

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <optional>
#include <string>
#include <vector>

namespace plumas::platform {

namespace {

core::FileIoError errorFromErrno() {
    switch (errno) {
    case EACCES:
    case EPERM:
        return core::FileIoError::PermissionDenied;
    case ENOENT:
        return core::FileIoError::NotFound;
    case ELOOP:
        return core::FileIoError::Symlink;
    default:
        return core::FileIoError::IoError;
    }
}

bool isValidPathString(const std::string& path) {
    if (path.empty() || path.size() > core::kMaxPathLength) {
        return false;
    }
    return path.find('\0') == std::string::npos;
}

bool isRegularFileDescriptor(const int fd) {
    struct stat status {};
    if (::fstat(fd, &status) != 0) {
        return false;
    }
    return S_ISREG(status.st_mode);
}

std::optional<std::string> readLimited(const int fd, const std::size_t maxBytes) {
    std::string content;
    content.reserve(4096);

    char buffer[8192];
    while (content.size() <= maxBytes) {
        const ssize_t bytesRead = ::read(fd, buffer, sizeof(buffer));
        if (bytesRead < 0) {
            return std::nullopt;
        }
        if (bytesRead == 0) {
            return content;
        }

        const std::size_t newSize = content.size() + static_cast<std::size_t>(bytesRead);
        if (newSize > maxBytes) {
            return std::nullopt;
        }

        content.append(buffer, static_cast<std::size_t>(bytesRead));
    }

    return std::nullopt;
}

} // namespace

core::FileReadResult readRegularTextFile(
    const std::filesystem::path& path,
    const std::size_t maxBytes) {
    core::FileReadResult result;

    if (!isValidPathString(path.string())) {
        result.error = core::FileIoError::InvalidPath;
        return result;
    }

    std::error_code error;
    if (std::filesystem::is_symlink(path, error)) {
        result.error = core::FileIoError::Symlink;
        return result;
    }

    if (std::filesystem::is_directory(path, error)) {
        result.error = core::FileIoError::NotRegularFile;
        return result;
    }

    const int fd = ::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        result.error = errorFromErrno();
        return result;
    }

    if (!isRegularFileDescriptor(fd)) {
        ::close(fd);
        result.error = core::FileIoError::NotRegularFile;
        return result;
    }

    const std::optional<std::string> content = readLimited(fd, maxBytes);
    ::close(fd);

    if (!content.has_value()) {
        result.error = core::FileIoError::TooLarge;
        return result;
    }

    result.content = *content;
    result.error = core::FileIoError::None;
    return result;
}

bool isSafeRegularFileSize(const std::filesystem::path& path, const std::size_t maxBytes) {
    if (!isValidPathString(path.string())) {
        return false;
    }

    std::error_code error;
    if (std::filesystem::is_symlink(path, error)) {
        return false;
    }

    const int fd = ::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }

    if (!isRegularFileDescriptor(fd)) {
        ::close(fd);
        return false;
    }

    struct stat status {};
    const bool withinLimit = ::fstat(fd, &status) == 0
        && static_cast<std::size_t>(status.st_size) <= maxBytes;
    ::close(fd);
    return withinLimit;
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

} // namespace plumas::platform
