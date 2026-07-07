#pragma once

#include "plumas/core/Config.hpp"
#include "plumas/core/Document.hpp"

#include <adwaita.h>
#include <gtk/gtk.h>

#include <memory>
#include <string>

namespace plumas::ui {

constexpr const char* kAppId = "com.plumas.EditorTexto";
constexpr const char* kWindowTitle = "Plumas Text Editor";
constexpr const char* kFooterTagline = "Lighter than Windows Notepad";
constexpr const char* kIconFileName = "plumas-icon.webp";
constexpr const char* kStylesFileName = "styles.css";

struct AppState {
    AdwApplication* app = nullptr;
    GtkWindow* window = nullptr;
    GtkWidget* toastOverlay = nullptr;
    GtkWidget* shell = nullptr;
    GtkWidget* textView = nullptr;
    GtkWidget* lineNumberView = nullptr;
    GtkWidget* pathLabel = nullptr;
    GtkWidget* saveButton = nullptr;
    GtkWidget* maximizeButton = nullptr;

    std::string pendingOpenPath;
    std::unique_ptr<core::Document> document;
    core::Config config;
    bool syncingEditor = false;
    bool syncingScroll = false;
    int lastLineCount = 0;
};

struct PendingAction {
    AppState* state = nullptr;
    void (*action)(AppState*) = nullptr;
};

struct PendingSave {
    AppState* state = nullptr;
    std::string path;
};

} // namespace plumas::ui
