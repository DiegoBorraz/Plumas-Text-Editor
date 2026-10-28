// libFuzzer target for the text helpers that guard what reaches GTK.
// Build: see tests/fuzz/README.md.

#include "plumas/core/FileIO.hpp"

#include <fuzzer/FuzzedDataProvider.h>
#include <glib.h>

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {

// isValidUtf8Text() decides what is handed to gtk_text_buffer_set_text(),
// which itself only accepts what g_utf8_validate() accepts. The two must agree
// byte for byte, or a file could load as an empty, "clean" document.
void checkUtf8(const std::string_view text) {
    const bool ours = plumas::core::isValidUtf8Text(text);
    const bool glib = text.empty()
        || g_utf8_validate(text.data(), static_cast<gssize>(text.size()), nullptr);
    if (ours != glib) {
        std::abort();
    }
}

void checkLineEndings(const std::string_view text) {
    const std::size_t newline = text.find('\n');
    const bool expected = newline != std::string_view::npos && newline > 0 && text[newline - 1] == '\r';
    if (plumas::core::usesCrlfLineEndings(text) != expected) {
        std::abort();
    }
}

// Naive reference: replace left to right, never re-scanning inserted text.
std::string naiveReplaceAll(std::string_view text, std::string_view find, std::string_view with) {
    std::string out;
    std::size_t position = 0;
    while (true) {
        const std::size_t match = text.find(find, position);
        if (match == std::string_view::npos) {
            break;
        }
        out.append(text.substr(position, match - position));
        out.append(with);
        position = match + find.size();
    }
    out.append(text.substr(position));
    return out;
}

void checkReplaceAll(FuzzedDataProvider& provider) {
    const std::string find = provider.ConsumeRandomLengthString(16);
    const std::string with = provider.ConsumeRandomLengthString(16);
    const std::string text = provider.ConsumeRemainingBytesAsString();
    if (find.empty()) {
        return;
    }

    constexpr std::size_t kMaxReplacements = 1'000'000;
    constexpr std::size_t kMaxOutput = 1U << 20;
    const plumas::core::TextReplaceResult result =
        plumas::core::replaceAllInText(text, find, with, kMaxReplacements, kMaxOutput, nullptr);
    if (result.limitReached || result.cancelled) {
        return;
    }

    const std::string expected = naiveReplaceAll(text, find, with);
    if (result.replacements == 0 ? expected != text : result.text != expected) {
        std::abort();
    }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string_view raw(reinterpret_cast<const char*>(data), size);
    checkUtf8(raw);
    checkLineEndings(raw);

    FuzzedDataProvider provider(data, size);
    checkReplaceAll(provider);
    return 0;
}
