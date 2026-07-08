#pragma once

#include "plumas/core/Config.hpp"
#include "plumas/core/Document.hpp"

#include <adwaita.h>
#include <gtk/gtk.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace plumas::ui {

constexpr const char* kAppId = "com.plumas.EditorTexto";
constexpr const char* kWindowTitle = "Plumas Text Editor";
constexpr int kAppTitleFontSizePt = 20;
constexpr const char* kFooterTagline = "Lighter than Windows Notepad. - https://github.com/DiegoBorraz";
constexpr const char* kIconFileName = "plumas-icon.webp";
constexpr const char* kStylesFileName = "styles.css";

struct ReplaceAllJob {
    std::atomic<bool> cancelRequested{false};
    std::thread worker;
};

struct AppState {
    AdwApplication* app = nullptr;
    GtkWindow* window = nullptr;
    GtkWidget* toastOverlay = nullptr;
    GtkWidget* shell = nullptr;
    GtkWidget* textView = nullptr;
    GtkWidget* lineGutter = nullptr;
    GtkWidget* editorScroll = nullptr;
    GtkWidget* pathLabel = nullptr;
    GtkWidget* saveButton = nullptr;
    GtkWidget* maximizeButton = nullptr;
    GtkWidget* searchBar = nullptr;
    GtkWidget* findEntry = nullptr;
    GtkWidget* replaceEntry = nullptr;

    std::string pendingOpenPath;
    std::unique_ptr<core::Document> document;
    core::Config config;
    bool syncingEditor = false;
    int monitorMaxWidth = 0;
    int monitorMaxHeight = 0;
    std::unique_ptr<ReplaceAllJob> replaceAllJob;
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
