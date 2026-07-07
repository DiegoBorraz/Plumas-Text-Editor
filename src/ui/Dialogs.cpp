#include "plumas/ui/Dialogs.hpp"

#include "plumas/ui/EditorView.hpp"
#include "plumas/ui/UiHelpers.hpp"

#include "plumas/core/FileIO.hpp"

#include <filesystem>
#include <memory>
#include <optional>

namespace plumas::ui {

namespace {

bool validateTextFilePath(AppState* state, const std::filesystem::path& path) {
    if (!core::isAllowedTextFilePath(path)) {
        showToast(state, "File type not allowed. Use text files.");
        return false;
    }
    return true;
}

GtkFileFilter* createAllowedTextFileFilter() {
    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Text files");

    for (const std::string& extension : core::allowedTextFileExtensionNames()) {
        const std::string pattern = "*." + extension;
        gtk_file_filter_add_pattern(filter, pattern.c_str());
    }

    gtk_file_filter_add_mime_type(filter, "text/plain");
    return filter;
}

void persistRecent(AppState* state, const std::string& path) {
    state->config.addRecentFile(path);
    state->config.save();
}

void openPath(AppState* state, const std::string& path) {
    if (path.empty()) {
        return;
    }

    if (!state->document->load(path)) {
        showToast(state, "Failed to open file.");
        return;
    }

    persistRecent(state, path);
    refreshEditor(state);
    showToast(state, "File opened.");
}

void onSaveDialogFinished(GObject* source, GAsyncResult* result, gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);
    GError* error = nullptr;
    GFile* file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source), result, &error);

    if (file == nullptr) {
        if (error != nullptr) {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
                showToast(state, "Failed to save file.");
            }
            g_error_free(error);
        }
        return;
    }

    char* path = g_file_get_path(file);
    if (path != nullptr) {
        trySavePath(state, path);
        g_free(path);
    }

    g_object_unref(file);
}

void onOpenDialogFinished(GObject* source, GAsyncResult* result, gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);
    GError* error = nullptr;
    GFile* file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, &error);

    if (file == nullptr) {
        if (error != nullptr) {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
                showToast(state, "Failed to open file.");
            }
            g_error_free(error);
        }
        return;
    }

    char* path = g_file_get_path(file);
    if (path != nullptr) {
        tryOpenPath(state, path);
        g_free(path);
    }

    g_object_unref(file);
}

void onUnsavedDialogFinished(GObject* source, GAsyncResult* result, gpointer userData) {
    std::unique_ptr<PendingAction> pending(static_cast<PendingAction*>(userData));
    AppState* state = pending->state;
    void (*action)(AppState*) = pending->action;
    const char* chosen =
        adw_alert_dialog_choose_finish(ADW_ALERT_DIALOG(source), result);

    if (chosen == nullptr || g_strcmp0(chosen, "cancel") == 0) {
        return;
    }

    if (g_strcmp0(chosen, "discard") == 0) {
        action(state);
        return;
    }

    if (state->document->filePath().empty()) {
        showSaveDialog(state);
        return;
    }

    if (!isEditorWithinSizeLimit(state)) {
        return;
    }

    syncEditorToDocument(state);
    if (state->document->save()) {
        persistRecent(state, state->document->filePath());
        updateStatus(state);
        action(state);
        return;
    }

    showToast(state, "Failed to save file.");
}

void onOverwriteDialogFinished(GObject* source, GAsyncResult* result, gpointer userData) {
    std::unique_ptr<PendingSave> pending(static_cast<PendingSave*>(userData));
    AppState* state = pending->state;
    const char* chosen =
        adw_alert_dialog_choose_finish(ADW_ALERT_DIALOG(source), result);

    if (chosen == nullptr || g_strcmp0(chosen, "cancel") == 0) {
        return;
    }

    performSave(state, pending->path);
}

} // namespace

void actionShowOpenDialog(AppState* state) {
    showOpenDialog(state);
}

void performSave(AppState* state, const std::string& path) {
    if (!isEditorWithinSizeLimit(state)) {
        return;
    }

    syncEditorToDocument(state);

    if (!state->document->saveAs(path)) {
        showToast(state, "Failed to save file.");
        return;
    }

    persistRecent(state, path);
    updateStatus(state);
    showToast(state, "File saved.");
}

void trySavePath(AppState* state, const std::string& path) {
    const std::optional<std::filesystem::path> normalized = core::normalizePath(path);
    if (!normalized.has_value()) {
        showToast(state, "Invalid file path.");
        return;
    }

    if (!validateTextFilePath(state, *normalized)) {
        return;
    }

    if (core::fileExistsForOverwrite(*normalized)) {
        auto pending = std::make_unique<PendingSave>(PendingSave{state, normalized->string()});
        showOverwriteDialog(state, std::move(pending));
        return;
    }

    performSave(state, normalized->string());
}

void saveDocument(AppState* state, bool saveAs) {
    if (saveAs || state->document->filePath().empty()) {
        showSaveDialog(state);
        return;
    }

    trySavePath(state, state->document->filePath());
}

void tryOpenPath(AppState* state, const std::string& path) {
    if (path.empty()) {
        return;
    }

    const std::optional<std::filesystem::path> normalized = core::normalizePath(path);
    if (!normalized.has_value()) {
        showToast(state, "Invalid file path.");
        return;
    }

    if (!validateTextFilePath(state, *normalized)) {
        return;
    }

    openPath(state, normalized->string());
}

void showOpenDialog(AppState* state) {
    GtkFileDialog* dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Open file");

    GtkFileFilter* filter = createAllowedTextFileFilter();
    GListStore* filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    g_object_unref(filters);
    g_object_unref(filter);

    gtk_file_dialog_open(
        dialog,
        state->window,
        nullptr,
        onOpenDialogFinished,
        state);
    g_object_unref(dialog);
}

void showSaveDialog(AppState* state) {
    GtkFileDialog* dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Save file");

    GtkFileFilter* filter = createAllowedTextFileFilter();
    GListStore* filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    gtk_file_dialog_set_default_filter(dialog, filter);
    g_object_unref(filters);
    g_object_unref(filter);

    if (!state->document->filePath().empty()) {
        GFile* initialFile = g_file_new_for_path(state->document->filePath().c_str());
        gtk_file_dialog_set_initial_file(dialog, initialFile);
        g_object_unref(initialFile);
    }

    gtk_file_dialog_save(
        dialog,
        state->window,
        nullptr,
        onSaveDialogFinished,
        state);
    g_object_unref(dialog);
}

void showUnsavedDialog(AppState* state, std::unique_ptr<PendingAction> pending) {
    PendingAction* rawPending = pending.release();
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
        rawPending);
}

void showOverwriteDialog(AppState* state, std::unique_ptr<PendingSave> pending) {
    PendingSave* rawPending = pending.release();
    AdwDialog* dialog = adw_alert_dialog_new(
        "Replace file?",
        "The file already exists. Do you want to replace it?");
    adw_alert_dialog_add_responses(
        ADW_ALERT_DIALOG(dialog),
        "cancel",
        "Cancel",
        "replace",
        "Replace",
        nullptr);
    adw_alert_dialog_set_response_appearance(
        ADW_ALERT_DIALOG(dialog),
        "replace",
        ADW_RESPONSE_DESTRUCTIVE);
    adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "cancel");
    adw_alert_dialog_choose(
        ADW_ALERT_DIALOG(dialog),
        GTK_WIDGET(state->window),
        nullptr,
        onOverwriteDialogFinished,
        rawPending);
}

void runWithUnsavedCheck(AppState* state, void (*action)(AppState*)) {
    if (!state->document->isDirty()) {
        action(state);
        return;
    }

    auto pending = std::make_unique<PendingAction>(PendingAction{state, action});
    showUnsavedDialog(state, std::move(pending));
}

void closeWindow(AppState* state) {
    gtk_window_destroy(state->window);
}

void newDocument(AppState* state) {
    state->document = std::make_unique<core::Document>();
    refreshEditor(state);
    showToast(state, "New document created.");
}

void openPendingPath(AppState* state) {
    if (state->pendingOpenPath.empty()) {
        return;
    }

    tryOpenPath(state, state->pendingOpenPath);
    state->pendingOpenPath.clear();
}

} // namespace plumas::ui
