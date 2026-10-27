#include "plumas/platform/FileBackend.hpp"

#include "plumas/core/FileIoTypes.hpp"

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

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

using core::isValidPathString;

mode_t defaultFileMode() {
    // umask() can only be read by setting it; nothing else in the process
    // creates files concurrently with a save.
    const mode_t mask = ::umask(0);
    ::umask(mask);
    return static_cast<mode_t>(0666) & ~mask;
}

mode_t targetFileMode(const std::filesystem::path& path, const core::FilePermissions permissions) {
    if (permissions == core::FilePermissions::OwnerOnly) {
        return S_IRUSR | S_IWUSR;
    }

    struct stat existing {};
    if (::stat(path.c_str(), &existing) == 0) {
        return existing.st_mode & 07777;
    }
    return defaultFileMode();
}

void syncDirectory(const std::filesystem::path& directory) {
    const int fd = ::open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd >= 0) {
        ::fsync(fd);
        ::close(fd);
    }
}

// O_NONBLOCK keeps open() from hanging on a FIFO until the fstat check below
// rejects it; reads from regular files ignore the flag.
int openForReading(const std::filesystem::path& path) {
    return ::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
}

bool isRegularFileDescriptor(const int fd) {
    struct stat status {};
    if (::fstat(fd, &status) != 0) {
        return false;
    }
    return S_ISREG(status.st_mode);
}

std::optional<std::string> readLimited(const int fd, const std::size_t maxBytes) {
    struct stat status {};
    if (::fstat(fd, &status) != 0 || status.st_size < 0
        || static_cast<std::size_t>(status.st_size) > maxBytes) {
        return std::nullopt;
    }

    // Size the buffer once from fstat so large files are not copied while the
    // string grows. The loop still enforces maxBytes if the file grows meanwhile.
    std::string content;
    content.resize(static_cast<std::size_t>(status.st_size));
    std::size_t used = 0;

    while (true) {
        if (used == content.size()) {
            if (content.size() >= maxBytes) {
                char probe = 0;
                const ssize_t extra = ::read(fd, &probe, 1);
                if (extra == 0) {
                    return content;
                }
                return std::nullopt;
            }
            content.resize(std::min(maxBytes, std::max<std::size_t>(content.size() * 2, 4096)));
        }

        const ssize_t bytesRead = ::read(fd, content.data() + used, content.size() - used);
        if (bytesRead < 0) {
            if (errno == EINTR) {
                continue;
            }
            return std::nullopt;
        }
        if (bytesRead == 0) {
            content.resize(used);
            return content;
        }
        used += static_cast<std::size_t>(bytesRead);
    }
}

} // namespace

core::FileReadResult readRegularTextFile(
    const std::filesystem::path& path,
    const std::size_t maxBytes) {
    core::FileReadResult result;

    if (!isValidPathString(path.native())) {
        result.error = core::FileIoError::InvalidPath;
        return result;
    }

    // O_NOFOLLOW reports a symlink as ELOOP and fstat rejects directories, so
    // no separate (racy) checks by path are needed.
    const int fd = openForReading(path);
    if (fd < 0) {
        result.error = errorFromErrno();
        return result;
    }

    if (!isRegularFileDescriptor(fd)) {
        ::close(fd);
        result.error = core::FileIoError::NotRegularFile;
        return result;
    }

    std::optional<std::string> content = readLimited(fd, maxBytes);
    ::close(fd);

    if (!content.has_value()) {
        result.error = core::FileIoError::TooLarge;
        return result;
    }

    result.content = std::move(*content);
    result.error = core::FileIoError::None;
    return result;
}

bool isSafeRegularFileSize(const std::filesystem::path& path, const std::size_t maxBytes) {
    if (!isValidPathString(path.native())) {
        return false;
    }

    const int fd = openForReading(path);
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

bool writeTextFileAtomic(
    const std::filesystem::path& path,
    const std::string_view content,
    const core::FilePermissions permissions) {
    if (!isValidPathString(path.native())) {
        return false;
    }

    std::error_code error;
    if (std::filesystem::is_symlink(path, error)) {
        return false;
    }

    const std::filesystem::path parent = path.parent_path();
    if (parent.empty()) {
        return false;
    }

    std::filesystem::create_directories(parent, error);

    std::string tempPath = (parent / (path.filename().string() + ".plumas-tmp-XXXXXX")).string();
    const int fd = ::mkstemp(tempPath.data());
    if (fd < 0) {
        return false;
    }

    const auto fail = [&]() {
        ::close(fd);
        ::unlink(tempPath.c_str());
        return false;
    };

    // mkstemp always creates 0600; without this every save would silently
    // make the user's file private.
    if (::fchmod(fd, targetFileMode(path, permissions)) != 0) {
        return fail();
    }

    std::size_t totalWritten = 0;
    while (totalWritten < content.size()) {
        const ssize_t written =
            ::write(fd, content.data() + totalWritten, content.size() - totalWritten);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return fail();
        }
        totalWritten += static_cast<std::size_t>(written);
    }

    if (::fsync(fd) != 0) {
        return fail();
    }

    if (::close(fd) != 0) {
        ::unlink(tempPath.c_str());
        return false;
    }

    if (::rename(tempPath.c_str(), path.c_str()) != 0) {
        ::unlink(tempPath.c_str());
        return false;
    }

    syncDirectory(parent);
    return true;
}

} // namespace plumas::platform
