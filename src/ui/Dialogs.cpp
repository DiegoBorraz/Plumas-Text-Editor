#include "plumas/ui/Dialogs.hpp"

#include "plumas/ui/EditorView.hpp"
#include "plumas/ui/SearchReplace.hpp"
#include "plumas/ui/Tabs.hpp"
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

    if (!state->activeTab->document->saveAs(path, content)) {
        showToast(state, "Failed to save file.");
        return false;
    }

    persistRecent(state, state->activeTab->document->filePath());
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

    // Save the tab the dialog was opened for, not whichever is active now.
    EditorTab* target = std::exchange(state->saveTarget, nullptr);
    if (!hasTab(state, target)) {
        if (file != nullptr) {
            g_object_unref(file);
        }
        g_clear_error(&error);
        runAfterSaveAction(state, false);
        return;
    }
    activateTab(state, target);

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

    // The answer is about the tab that was asked about; it may have been
    // closed, or another tab activated, while the dialog was open.
    if (!hasTab(state, pending->tab)) {
        return;
    }
    activateTab(state, pending->tab);

    if (g_strcmp0(chosen, "discard") == 0) {
        pending->action(state);
        return;
    }

    if (state->activeTab->document->filePath().empty()) {
        // The save dialog is asynchronous; run the action once it saves.
        state->afterSaveAction = pending->action;
        showSaveDialog(state);
        return;
    }

    if (performSave(state, state->activeTab->document->filePath())) {
        pending->action(state);
    }
}

} // namespace

void saveDocument(AppState* state, const bool saveAs) {
    if (saveAs || state->activeTab->document->filePath().empty()) {
        showSaveDialog(state);
        return;
    }

    performSave(state, state->activeTab->document->filePath());
}

void tryOpenPath(AppState* state, const std::string& path) {
    const std::optional<std::string> valid = validateTextFilePath(state, path);
    if (!valid.has_value()) {
        return;
    }

    // Opening a file that is already open just shows its tab.
    if (EditorTab* existing = findTabByPath(state, *valid)) {
        activateTab(state, existing);
        return;
    }

    auto document = std::make_unique<core::Document>();
    const std::optional<std::string> content = document->load(*valid);
    if (!content.has_value()) {
        showToast(state, "Failed to open file.");
        return;
    }

    const bool usesCrlf = core::usesCrlfLineEndings(*content);
    if (isReusableTab(*state->activeTab)) {
        // Like Notepad, a blank untitled tab is replaced instead of kept.
        abandonReplaceAll(state);
        state->activeTab->document = std::move(document);
        state->activeTab->usesCrlf = usesCrlf;
        setEditorText(state, *content);
        updateTabLabel(state->activeTab);
    } else {
        addTab(state, std::move(document), *content, usesCrlf);
    }
    persistRecent(state, state->activeTab->document->filePath());
}

void showOpenDialog(AppState* state) {
    GtkFileDialog* dialog = createTextFileDialog("Open file");
    gtk_file_dialog_open(dialog, state->window, nullptr, onOpenDialogFinished, state);
    g_object_unref(dialog);
}

void showSaveDialog(AppState* state) {
    GtkFileDialog* dialog = createTextFileDialog("Save file");
    state->saveTarget = state->activeTab;

    if (!state->activeTab->document->filePath().empty()) {
        GFile* initialFile = g_file_new_for_path(state->activeTab->document->filePath().c_str());
        gtk_file_dialog_set_initial_file(dialog, initialFile);
        g_object_unref(initialFile);
    }

    gtk_file_dialog_save(dialog, state->window, nullptr, onSaveDialogFinished, state);
    g_object_unref(dialog);
}

void showUnsavedDialog(AppState* state, const AppAction action) {
    auto* pending = new PendingAction{state, action, state->activeTab};
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

} // namespace plumas::ui
