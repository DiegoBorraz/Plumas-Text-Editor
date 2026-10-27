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
#include <vector>

namespace plumas::ui {

constexpr const char* kAppId = "com.plumas.EditorTexto";
constexpr const char* kWindowTitle = "Plumas Text Editor";
constexpr const char* kAppShortName = "Plumas";
constexpr const char* kUntitledName = "Untitled";
constexpr int kDefaultZoomPercent = 100;
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

// One open document. Every tab owns a text buffer; the single text view shows
// the active tab's buffer, so extra tabs cost only their text and undo history.
struct EditorTab {
    std::unique_ptr<core::Document> document;
    GtkTextBuffer* buffer = nullptr;
    // Informational only: shows the convention of the file that was opened.
    bool usesCrlf = false;
    GtkWidget* tabWidget = nullptr;
    GtkWidget* tabLabel = nullptr;

    EditorTab() = default;
    EditorTab(const EditorTab&) = delete;
    EditorTab& operator=(const EditorTab&) = delete;
    ~EditorTab() {
        if (buffer != nullptr) {
            g_object_unref(buffer);
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
    GtkWidget* tabBox = nullptr;
    GtkWidget* maximizeButton = nullptr;
    GtkWidget* statusBar = nullptr;
    GtkWidget* positionLabel = nullptr;
    GtkWidget* zoomLabel = nullptr;
    GtkWidget* lineEndingLabel = nullptr;
    GtkCssProvider* zoomProvider = nullptr;
    int zoomPercent = kDefaultZoomPercent;
    GtkWidget* searchBar = nullptr;
    GtkWidget* findEntry = nullptr;
    GtkWidget* replaceEntry = nullptr;

    // Runs once the save started from the unsaved-changes dialog succeeds.
    AppAction afterSaveAction = nullptr;
    std::vector<std::unique_ptr<EditorTab>> tabs;
    // Never null while the window exists.
    EditorTab* activeTab = nullptr;
    core::Config config;
    bool syncingEditor = false;
    std::unique_ptr<ReplaceAllJob> replaceAllJob;
};

struct PendingAction {
    AppState* state = nullptr;
    AppAction action = nullptr;
};

} // namespace plumas::ui
