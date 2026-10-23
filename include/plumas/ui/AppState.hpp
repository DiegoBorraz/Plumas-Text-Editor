#pragma once

#include "plumas/core/Config.hpp"
#include "plumas/core/Document.hpp"
#include "plumas/core/FileIoTypes.hpp"

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

using GCharPtr = std::unique_ptr<char, decltype(&g_free)>;

struct AppState;
using AppAction = void (*)(AppState*);

// Shared between the main thread, the worker and the dialog/idle callbacks.
// Every holder keeps a shared_ptr, so no callback can outlive it.
struct ReplaceAllTask {
    AppState* state = nullptr;
    GCharPtr sourceText{nullptr, g_free};
    std::string findText;
    std::string replaceText;
    core::TextReplaceResult result;
    std::atomic<bool> cancelRequested{false};
    // Main thread only.
    bool finished = false;
    bool abandoned = false;
    AdwDialog* progressDialog = nullptr;
};

struct ReplaceAllJob {
    std::shared_ptr<ReplaceAllTask> task;
    std::thread worker;

    ReplaceAllJob() = default;
    ReplaceAllJob(const ReplaceAllJob&) = delete;
    ReplaceAllJob& operator=(const ReplaceAllJob&) = delete;

    // Reached with a running worker only when the window goes away mid-job.
    ~ReplaceAllJob() {
        if (worker.joinable()) {
            task->abandoned = true;
            task->cancelRequested.store(true);
            worker.join();
        }
    }
};

struct AppState {
    AdwApplication* app = nullptr;
    GtkWindow* window = nullptr;
    GtkWidget* toastOverlay = nullptr;
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
    // Runs once the save started from the unsaved-changes dialog succeeds.
    AppAction afterSaveAction = nullptr;
    std::unique_ptr<core::Document> document;
    core::Config config;
    bool syncingEditor = false;
    std::unique_ptr<ReplaceAllJob> replaceAllJob;
};

struct PendingAction {
    AppState* state = nullptr;
    AppAction action = nullptr;
};

} // namespace plumas::ui
