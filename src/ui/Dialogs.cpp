#include "plumas/ui/Dialogs.hpp"

#include "plumas/ui/EditorView.hpp"
#include "plumas/ui/SearchReplace.hpp"
#include "plumas/ui/UiHelpers.hpp"

#include "plumas/core/FileIO.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace plumas::ui {

namespace {

// Normalizes a user-supplied path and rejects non-text extensions, reporting
// the reason to the user.
std::optional<std::string> validateTextFilePath(AppState* state, const std::string& path) {
    const std::optional<std::filesystem::path> normalized = core::normalizePath(path);
    if (!normalized.has_value()) {
        showToast(state, "Invalid file path.");
        return std::nullopt;
    }

    if (!core::isAllowedTextFilePath(*normalized)) {
        showToast(state, "File type not allowed. Use text files.");
        return std::nullopt;
    }

    return core::pathToUtf8(*normalized);
}

GtkFileDialog* createTextFileDialog(const char* title) {
    GtkFileDialog* dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, title);

    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Text files");
    for (const std::string& extension : core::allowedTextFileExtensionNames()) {
        const std::string pattern = "*." + extension;
        gtk_file_filter_add_pattern(filter, pattern.c_str());
    }
    gtk_file_filter_add_mime_type(filter, "text/plain");

    GListStore* filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    gtk_file_dialog_set_default_filter(dialog, filter);
    g_object_unref(filters);
    g_object_unref(filter);
    return dialog;
}

void persistRecent(AppState* state, const std::string& path) {
    state->config.addRecentFile(path);
    state->config.save();
}

void runAfterSaveAction(AppState* state, const bool saved) {
    const AppAction action = std::exchange(state->afterSaveAction, nullptr);
    if (saved && action != nullptr) {
        action(state);
    }
}

bool performSave(AppState* state, const std::string& path) {
    const GCharPtr text = copyEditorText(state);
    const std::string_view content = text != nullptr ? text.get() : "";

    if (content.size() > core::kMaxFileSizeBytes) {
        showToast(state, "Document exceeds the 50 MB limit.");
        return false;
    }

    if (!state->document->saveAs(path, content)) {
        showToast(state, "Failed to save file.");
        return false;
    }

    persistRecent(state, state->document->filePath());
    updateStatus(state);
    showToast(state, "File saved.");
    return true;
}

// Takes the GFile chosen in a file dialog, or nullptr if it was dismissed.
std::optional<std::string> takeDialogPath(AppState* state, GFile* file, GError* error) {
    if (file == nullptr) {
        if (error != nullptr) {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
                showToast(state, "Could not use the selected file.");
            }
            g_error_free(error);
        }
        return std::nullopt;
    }

    GCharPtr path(g_file_get_path(file), g_free);
    g_object_unref(file);
    if (path == nullptr) {
        return std::nullopt;
    }
    return std::string(path.get());
}

void onSaveDialogFinished(GObject* source, GAsyncResult* result, gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);
    GError* error = nullptr;
    GFile* file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source), result, &error);

    // The file dialog already asked before replacing an existing file.
    bool saved = false;
    if (const std::optional<std::string> path = takeDialogPath(state, file, error)) {
        if (const std::optional<std::string> valid = validateTextFilePath(state, *path)) {
            saved = performSave(state, *valid);
        }
    }
    runAfterSaveAction(state, saved);
}

void onOpenDialogFinished(GObject* source, GAsyncResult* result, gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);
    GError* error = nullptr;
    GFile* file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, &error);

    if (const std::optional<std::string> path = takeDialogPath(state, file, error)) {
        tryOpenPath(state, *path);
    }
}

void onUnsavedDialogFinished(GObject* source, GAsyncResult* result, gpointer userData) {
    const std::unique_ptr<PendingAction> pending(static_cast<PendingAction*>(userData));
    AppState* state = pending->state;
    const char* chosen = adw_alert_dialog_choose_finish(ADW_ALERT_DIALOG(source), result);

    if (chosen == nullptr || g_strcmp0(chosen, "cancel") == 0) {
        return;
    }

    if (g_strcmp0(chosen, "discard") == 0) {
        pending->action(state);
        return;
    }

    if (state->document->filePath().empty()) {
        // The save dialog is asynchronous; run the action once it saves.
        state->afterSaveAction = pending->action;
        showSaveDialog(state);
        return;
    }

    if (performSave(state, state->document->filePath())) {
        pending->action(state);
    }
}

} // namespace

void saveDocument(AppState* state, const bool saveAs) {
    if (saveAs || state->document->filePath().empty()) {
        showSaveDialog(state);
        return;
    }

    performSave(state, state->document->filePath());
}

void tryOpenPath(AppState* state, const std::string& path) {
    const std::optional<std::string> valid = validateTextFilePath(state, path);
    if (!valid.has_value()) {
        return;
    }

    auto document = std::make_unique<core::Document>();
    const std::optional<std::string> content = document->load(*valid);
    if (!content.has_value()) {
        showToast(state, "Failed to open file.");
        return;
    }

    abandonReplaceAll(state);
    state->document = std::move(document);
    setEditorText(state, content->c_str());
    persistRecent(state, state->document->filePath());
    showToast(state, "File opened.");
}

void showOpenDialog(AppState* state) {
    GtkFileDialog* dialog = createTextFileDialog("Open file");
    gtk_file_dialog_open(dialog, state->window, nullptr, onOpenDialogFinished, state);
    g_object_unref(dialog);
}

void showSaveDialog(AppState* state) {
    GtkFileDialog* dialog = createTextFileDialog("Save file");

    if (!state->document->filePath().empty()) {
        GFile* initialFile = g_file_new_for_path(state->document->filePath().c_str());
        gtk_file_dialog_set_initial_file(dialog, initialFile);
        g_object_unref(initialFile);
    }

    gtk_file_dialog_save(dialog, state->window, nullptr, onSaveDialogFinished, state);
    g_object_unref(dialog);
}

void showUnsavedDialog(AppState* state, const AppAction action) {
    auto* pending = new PendingAction{state, action};
    AdwDialog* dialog = adw_alert_dialog_new(
        "Unsaved changes",
        "Do you want to save your changes before continuing?");
    adw_alert_dialog_add_responses(
        ADW_ALERT_DIALOG(dialog),
        "cancel",
        "Cancel",
        "discard",
        "Discard",
        "save",
        "Save",
        nullptr);
    adw_alert_dialog_set_response_appearance(
        ADW_ALERT_DIALOG(dialog),
        "discard",
        ADW_RESPONSE_DESTRUCTIVE);
    adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "cancel");
    adw_alert_dialog_choose(
        ADW_ALERT_DIALOG(dialog),
        GTK_WIDGET(state->window),
        nullptr,
        onUnsavedDialogFinished,
        pending);
}

void runWithUnsavedCheck(AppState* state, const AppAction action) {
    if (!state->document->isDirty()) {
        action(state);
        return;
    }

    showUnsavedDialog(state, action);
}

void closeWindow(AppState* state) {
    gtk_window_destroy(state->window);
}

void newDocument(AppState* state) {
    abandonReplaceAll(state);
    state->document = std::make_unique<core::Document>();
    setEditorText(state, "");
    showToast(state, "New document created.");
}

void openPendingPath(AppState* state) {
    const std::string path = std::exchange(state->pendingOpenPath, {});
    if (!path.empty()) {
        tryOpenPath(state, path);
    }
}

} // namespace plumas::ui
