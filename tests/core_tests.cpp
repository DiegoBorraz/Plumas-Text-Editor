// Dependency-free tests for plumas_core. Run with `ctest` from the build dir.

#include "plumas/core/Config.hpp"
#include "plumas/core/Document.hpp"
#include "plumas/core/FileIO.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <optional>
#include <random>
#include <string>
#include <vector>

#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace fs = std::filesystem;
using namespace plumas::core;

namespace {

int gFailures = 0;

#define CHECK(condition)                                                              \
    do {                                                                              \
        if (!(condition)) {                                                           \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #condition \
                      << "\n";                                                        \
            ++gFailures;                                                              \
        }                                                                             \
    } while (false)

TextReplaceResult replaceAll(const std::string& text, const std::string& find, const std::string& with) {
    return replaceAllInText(text, find, with, kMaxReplaceAllReplacements, kMaxFileSizeBytes, nullptr);
}

struct TempDir {
    fs::path path;

    TempDir() {
        path = fs::temp_directory_path()
            / ("plumas-tests-" + std::to_string(std::random_device{}()));
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code error;
        fs::remove_all(path, error);
    }
};

std::string readAll(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void testReplaceAll() {
    TextReplaceResult result = replaceAll("a-b-c", "-", "+");
    CHECK(result.text == "a+b+c");
    CHECK(result.replacements == 2);

    result = replaceAll("aaaa", "aa", "b");
    CHECK(result.text == "bb");
    CHECK(result.replacements == 2);

    // A replacement containing the search text must not be searched again.
    result = replaceAll("x", "x", "xx");
    CHECK(result.text == "xx");
    CHECK(result.replacements == 1);

    result = replaceAll("abc", "z", "y");
    CHECK(result.replacements == 0);
    CHECK(!result.limitReached && !result.cancelled);

    result = replaceAll("abc", "", "y");
    CHECK(result.replacements == 0);

    result = replaceAll("héllo wörld", "ö", "o");
    CHECK(result.text == "héllo world");

    result = replaceAllInText("aaaa", "a", "b", 2, kMaxFileSizeBytes, nullptr);
    CHECK(result.limitReached);
    CHECK(result.text.empty());

    // Exactly at the limit with no further match is a normal success.
    result = replaceAllInText("aa", "a", "b", 2, kMaxFileSizeBytes, nullptr);
    CHECK(!result.limitReached);
    CHECK(result.text == "bb");

    result = replaceAllInText("aaaa", "a", "bbbb", 100, 8, nullptr);
    CHECK(result.limitReached);

    const std::atomic<bool> cancelled{true};
    result = replaceAllInText("aaaa", "a", "b", 100, kMaxFileSizeBytes, &cancelled);
    CHECK(result.cancelled);
}

void testReplaceAllIsLinear() {
    // One million matches finished instantly with the single-pass version and
    // took minutes with the old in-place replace.
    const std::string text(1'000'000, 'a');
    const auto start = std::chrono::steady_clock::now();
    const TextReplaceResult result = replaceAll(text, "a", "bb");
    const auto elapsed = std::chrono::steady_clock::now() - start;
    CHECK(result.replacements == 1'000'000);
    CHECK(result.text.size() == 2'000'000);
    CHECK(elapsed < std::chrono::seconds(2));
}

void testPaths() {
    CHECK(isAllowedTextFilePath("notes.txt"));
    CHECK(isAllowedTextFilePath("README.MD"));
    CHECK(!isAllowedTextFilePath("program.exe"));
    CHECK(!isAllowedTextFilePath("Makefile"));
    CHECK(!isAllowedTextFilePath("."));

    CHECK(!normalizePath("").has_value());
    CHECK(!normalizePath(std::string("a\0b", 3)).has_value());
    CHECK(!normalizePath(std::string(kMaxPathLength + 1, 'a')).has_value());

    const std::string utf8 = "relat\xC3\xB3rio.txt";
    CHECK(pathToUtf8(pathFromUtf8(utf8)) == utf8);
}

void testReadLimits() {
    TempDir dir;
    const fs::path file = dir.path / "limit.txt";
    std::ofstream(file, std::ios::binary) << "12345";

    FileReadResult result = readTextFile(file, 5);
    CHECK(result.error == FileIoError::None);
    CHECK(result.content == "12345");

    result = readTextFile(file, 4);
    CHECK(result.error == FileIoError::TooLarge);

    result = readTextFile(dir.path, 100);
    CHECK(result.error == FileIoError::NotRegularFile);

    result = readTextFile(dir.path / "missing.txt", 100);
    CHECK(result.error == FileIoError::NotFound);

    const fs::path empty = dir.path / "empty.txt";
    std::ofstream(empty, std::ios::binary).flush();
    result = readTextFile(empty, 100);
    CHECK(result.error == FileIoError::None);
    CHECK(result.content.empty());
}

void testUtf8Validation() {
    CHECK(isValidUtf8Text(""));
    CHECK(isValidUtf8Text("plain ascii\n"));
    CHECK(isValidUtf8Text("relat\xC3\xB3rio \xE2\x82\xAC \xF0\x9F\x98\x80"));
    CHECK(!isValidUtf8Text(std::string("a\0b", 3)));
    CHECK(!isValidUtf8Text("caf\xE9"));           // Latin-1
    CHECK(!isValidUtf8Text("\xC0\xAF"));          // overlong '/'
    CHECK(!isValidUtf8Text("\xE0\x80\xAF"));     // overlong '/'
    CHECK(!isValidUtf8Text("\xED\xA0\x80"));     // surrogate
    CHECK(!isValidUtf8Text("\xF4\x90\x80\x80")); // above U+10FFFF
    CHECK(!isValidUtf8Text("\xE2\x82"));          // truncated
    CHECK(!isValidUtf8Text("\x80"));              // stray continuation
}

void testLineEndingDetection() {
    CHECK(usesCrlfLineEndings("a\r\nb\n"));
    CHECK(!usesCrlfLineEndings("a\nb\r\n"));
    CHECK(!usesCrlfLineEndings("no newline"));
    CHECK(!usesCrlfLineEndings("\n"));
    CHECK(!usesCrlfLineEndings(""));
}

void testDocumentRejectsNonText() {
    TempDir dir;
    const fs::path withNul = dir.path / "nul.txt";
    std::ofstream(withNul, std::ios::binary) << std::string("head\0tail", 9);
    const fs::path latin1 = dir.path / "latin1.txt";
    std::ofstream(latin1, std::ios::binary) << "caf\xE9";

    // Loading these would show an empty or cut buffer that saves over the file.
    Document document;
    CHECK(!document.load(pathToUtf8(withNul)).has_value());
    CHECK(!document.load(pathToUtf8(latin1)).has_value());
    CHECK(document.filePath().empty());
}

void testDocumentRoundTrip() {
    TempDir dir;
    const std::string path = pathToUtf8(dir.path / "relat\xC3\xB3rio.txt");

    Document document;
    CHECK(document.saveAs(path, "linha 1\nlinha 2\n"));
    CHECK(!document.isDirty());
    CHECK(document.filePath() == pathToUtf8(normalizePath(path).value()));

    Document reopened;
    const std::optional<std::string> content = reopened.load(path);
    CHECK(content.has_value() && *content == "linha 1\nlinha 2\n");

    CHECK(!document.saveAs(pathToUtf8(dir.path / "evil.exe"), "x"));
    CHECK(!fs::exists(dir.path / "evil.exe"));
    CHECK(!reopened.load(pathToUtf8(dir.path / "missing.txt")).has_value());

    // No temp files left next to the document.
    std::size_t entries = 0;
    for ([[maybe_unused]] const auto& entry : fs::directory_iterator(dir.path)) {
        ++entries;
    }
    CHECK(entries == 1);
}

#ifndef _WIN32
mode_t modeOf(const fs::path& path) {
    struct stat status {};
    ::stat(path.c_str(), &status);
    return status.st_mode & 07777;
}

void testSavePreservesPermissions() {
    TempDir dir;
    const fs::path file = dir.path / "shared.txt";
    std::ofstream(file) << "old";
    ::chmod(file.c_str(), 0644);

    CHECK(writeTextFileAtomic(file, "new", FilePermissions::PreserveOrDefault));
    CHECK(modeOf(file) == 0644);
    CHECK(readAll(file) == "new");

    const fs::path config = dir.path / "config.json";
    CHECK(writeTextFileAtomic(config, "{}", FilePermissions::OwnerOnly));
    CHECK(modeOf(config) == 0600);

    // Saving through a symlink is refused instead of replacing the link.
    const fs::path link = dir.path / "link.txt";
    fs::create_symlink(file, link);
    CHECK(!writeTextFileAtomic(link, "x", FilePermissions::PreserveOrDefault));
    CHECK(readAll(file) == "new");
    CHECK(readTextFile(link, 100).error == FileIoError::Symlink);
}

void testDocumentFollowsSymlink() {
    TempDir dir;
    const fs::path target = dir.path / "target.txt";
    std::ofstream(target) << "original";
    const fs::path link = dir.path / "link.txt";
    fs::create_symlink(target, link);

    // A document opened through a link edits the target and keeps the link.
    Document document;
    const std::optional<std::string> content = document.load(pathToUtf8(link));
    CHECK(content.has_value() && *content == "original");
    CHECK(document.filePath() == pathToUtf8(fs::canonical(target)));
    CHECK(document.saveAs(pathToUtf8(link), "edited"));
    CHECK(fs::is_symlink(link));
    CHECK(readAll(target) == "edited");
}

void testFifoDoesNotBlock() {
    TempDir dir;
    const fs::path fifo = dir.path / "pipe.txt";
    CHECK(::mkfifo(fifo.c_str(), 0600) == 0);

    // Without a writer, a blocking open() would hang here forever.
    CHECK(readTextFile(fifo, 100).error == FileIoError::NotRegularFile);
    CHECK(!isSafeRegularFileSize(fifo, 100));
}

void testConfig() {
    TempDir dir;
    ::setenv("XDG_CONFIG_HOME", dir.path.c_str(), 1);

    Config config;
    CHECK(config.load());
    for (int index = 0; index < 15; ++index) {
        config.addRecentFile(pathToUtf8(dir.path / ("file" + std::to_string(index) + ".txt")));
    }
    config.addRecentFile(pathToUtf8(dir.path / "blocked.exe"));
    config.addRecentFile(pathToUtf8(dir.path / "file3.txt"));
    CHECK(config.recentFiles().size() == 10);
    CHECK(config.recentFiles().front().ends_with("file3.txt"));
    CHECK(config.save());
    CHECK(modeOf(Config::defaultPath()) == 0600);

    Config reloaded;
    CHECK(reloaded.load());
    CHECK(reloaded.recentFiles() == config.recentFiles());

    // Linux file names need not be UTF-8; JSON cannot hold them.
    Config nonUtf8;
    nonUtf8.addRecentFile(pathToUtf8(dir.path) + "/caf\xE9.txt");
    CHECK(nonUtf8.recentFiles().empty());
    CHECK(nonUtf8.save());

    // Recent files survive the rename from Plumas: the old config is read
    // when the new one does not exist yet, and is left in place.
    const fs::path legacyFile = dir.path / "legacy-notes.txt";
    fs::create_directories(Config::legacyPath().parent_path());
    std::ofstream(Config::legacyPath())
        << R"({"recentFiles": [")" << pathToUtf8(legacyFile) << R"("]})";
    fs::remove(Config::defaultPath());
    Config migrated;
    CHECK(migrated.load());
    CHECK(migrated.recentFiles().size() == 1);
    CHECK(migrated.recentFiles().front() == pathToUtf8(normalizePath(pathToUtf8(legacyFile)).value()));
    CHECK(migrated.save());
    CHECK(fs::exists(Config::defaultPath()));
    CHECK(fs::exists(Config::legacyPath()));
    CHECK(Config::defaultPath().parent_path().filename() == "pluma-text");

    std::ofstream(Config::defaultPath()) << "{ not json";
    Config broken;
    CHECK(!broken.load());
    CHECK(broken.recentFiles().empty());
}
#endif

} // namespace

int main() {
    const std::vector<std::pair<const char*, std::function<void()>>> tests = {
        {"replaceAll", testReplaceAll},
        {"replaceAllIsLinear", testReplaceAllIsLinear},
        {"paths", testPaths},
        {"readLimits", testReadLimits},
        {"utf8Validation", testUtf8Validation},
        {"lineEndingDetection", testLineEndingDetection},
        {"documentRejectsNonText", testDocumentRejectsNonText},
        {"documentRoundTrip", testDocumentRoundTrip},
#ifndef _WIN32
        {"fifoDoesNotBlock", testFifoDoesNotBlock},
        {"savePreservesPermissions", testSavePreservesPermissions},
        {"documentFollowsSymlink", testDocumentFollowsSymlink},
        {"config", testConfig},
#endif
    };

    for (const auto& [name, test] : tests) {
        const int before = gFailures;
        test();
        std::cout << (gFailures == before ? "PASS " : "FAIL ") << name << "\n";
    }
    return gFailures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
