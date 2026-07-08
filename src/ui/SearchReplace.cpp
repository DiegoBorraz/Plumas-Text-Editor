#include "plumas/ui/SearchReplace.hpp"

#include "plumas/ui/EditorView.hpp"
#include "plumas/ui/UiHelpers.hpp"

#include "plumas/core/FileIO.hpp"

#include <string>
#include <utility>

namespace plumas::ui {

namespace {

constexpr const char kSearchHighlightTag[] = "search-highlight";
constexpr const char kSearchPositionMark[] = "plumas-search-position";

GtkTextTag* ensureSearchHighlightTag(AppState* state) {
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->textView));
    GtkTextTagTable* tagTable = gtk_text_buffer_get_tag_table(buffer);
    GtkTextTag* tag = gtk_text_tag_table_lookup(tagTable, kSearchHighlightTag);
    if (tag != nullptr) {
        return tag;
    }

    tag = gtk_text_buffer_create_tag(
        buffer,
        kSearchHighlightTag,
        "background",
        "#fff59d",
        nullptr);
    return tag;
}

void clearSearchHighlight(AppState* state) {
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->textView));
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    gtk_text_buffer_remove_tag_by_name(buffer, kSearchHighlightTag, &start, &end);
}

void highlightMatch(AppState* state, const GtkTextIter* matchStart, const GtkTextIter* matchEnd) {
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->textView));
    ensureSearchHighlightTag(state);
    clearSearchHighlight(state);
    gtk_text_buffer_apply_tag(buffer, ensureSearchHighlightTag(state), matchStart, matchEnd);
}

std::string getFindText(const AppState* state) {
    const char* text = gtk_editable_get_text(GTK_EDITABLE(state->findEntry));
    return text != nullptr ? text : "";
}

std::string getReplaceText(const AppState* state) {
    const char* text = gtk_editable_get_text(GTK_EDITABLE(state->replaceEntry));
    return text != nullptr ? text : "";
}

bool findMatch(AppState* state, bool backward, GtkTextIter* matchStart, GtkTextIter* matchEnd) {
    const std::string searchText = getFindText(state);
    if (searchText.empty()) {
        return false;
    }

    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->textView));
    const GtkTextSearchFlags flags = static_cast<GtkTextSearchFlags>(
        GTK_TEXT_SEARCH_VISIBLE_ONLY | GTK_TEXT_SEARCH_TEXT_ONLY);

    GtkTextIter searchFrom;
    if (gtk_text_buffer_get_has_selection(buffer)) {
        if (backward) {
            gtk_text_buffer_get_selection_bounds(buffer, &searchFrom, nullptr);
            if (!gtk_text_iter_is_start(&searchFrom)) {
                gtk_text_iter_backward_char(&searchFrom);
            }
        } else {
            gtk_text_buffer_get_selection_bounds(buffer, nullptr, &searchFrom);
            if (!gtk_text_iter_is_end(&searchFrom)) {
                gtk_text_iter_forward_char(&searchFrom);
            }
        }
    } else if (GtkTextMark* mark = gtk_text_buffer_get_mark(buffer, kSearchPositionMark);
               mark != nullptr) {
        gtk_text_buffer_get_iter_at_mark(buffer, &searchFrom, mark);
    } else {
        gtk_text_buffer_get_iter_at_mark(buffer, &searchFrom, gtk_text_buffer_get_insert(buffer));
    }

    if (backward) {
        if (gtk_text_iter_backward_search(
                &searchFrom,
                searchText.c_str(),
                flags,
                matchStart,
                matchEnd,
                nullptr)) {
            return true;
        }

        GtkTextIter endIter;
        gtk_text_buffer_get_end_iter(buffer, &endIter);
        return gtk_text_iter_backward_search(
            &endIter,
            searchText.c_str(),
            flags,
            matchStart,
            matchEnd,
            nullptr);
    }

    if (gtk_text_iter_forward_search(
            &searchFrom,
            searchText.c_str(),
            flags,
            matchStart,
            matchEnd,
            nullptr)) {
        return true;
    }

    GtkTextIter startIter;
    gtk_text_buffer_get_start_iter(buffer, &startIter);
    return gtk_text_iter_forward_search(
        &startIter,
        searchText.c_str(),
        flags,
        matchStart,
        matchEnd,
        nullptr);
}

void selectMatch(AppState* state, const GtkTextIter* matchStart, const GtkTextIter* matchEnd) {
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->textView));
    gtk_text_buffer_select_range(buffer, matchStart, matchEnd);
    gtk_text_buffer_move_mark(buffer, gtk_text_buffer_get_mark(buffer, kSearchPositionMark), matchEnd);
    highlightMatch(state, matchStart, matchEnd);

    GtkTextIter scrollTarget = *matchStart;
    gtk_text_view_scroll_to_iter(
        GTK_TEXT_VIEW(state->textView),
        &scrollTarget,
        0.1,
        FALSE,
        0.0,
        0.0);
}

bool performFind(AppState* state, bool backward) {
    GtkTextIter matchStart;
    GtkTextIter matchEnd;
    if (!findMatch(state, backward, &matchStart, &matchEnd)) {
        clearSearchHighlight(state);
        showToast(state, "No matches found.");
        return false;
    }

    selectMatch(state, &matchStart, &matchEnd);
    return true;
}

void hideSearchReplaceBar(AppState* state) {
    if (state->searchBar == nullptr || !gtk_widget_get_visible(state->searchBar)) {
        return;
    }

    gtk_widget_set_visible(state->searchBar, FALSE);
    clearSearchHighlight(state);
    gtk_widget_grab_focus(state->textView);
}

void showSearchReplaceBar(AppState* state) {
    gtk_widget_set_visible(state->searchBar, TRUE);
    gtk_widget_grab_focus(state->findEntry);
    gtk_editable_select_region(GTK_EDITABLE(state->findEntry), 0, -1);
}

void onFindNextClicked(GtkButton* /*button*/, gpointer userData) {
    performFind(static_cast<AppState*>(userData), false);
}

void onFindPreviousClicked(GtkButton* /*button*/, gpointer userData) {
    performFind(static_cast<AppState*>(userData), true);
}

void onReplaceClicked(GtkButton* /*button*/, gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);
    const std::string findText = getFindText(state);
    const std::string replaceText = getReplaceText(state);

    if (findText.empty()) {
        return;
    }

    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->textView));
    GtkTextIter selectionStart;
    GtkTextIter selectionEnd;
    bool hasSelection = gtk_text_buffer_get_has_selection(buffer);

    if (hasSelection) {
        gtk_text_buffer_get_selection_bounds(buffer, &selectionStart, &selectionEnd);
        g_autofree gchar* selected =
            gtk_text_buffer_get_text(buffer, &selectionStart, &selectionEnd, FALSE);
        hasSelection = selected != nullptr && findText == selected;
    }

    if (!hasSelection && !performFind(state, false)) {
        return;
    }

    gtk_text_buffer_get_selection_bounds(buffer, &selectionStart, &selectionEnd);
    gtk_text_buffer_delete(buffer, &selectionStart, &selectionEnd);
    gtk_text_buffer_insert(buffer, &selectionStart, replaceText.c_str(), -1);

    GtkTextIter replaceEnd = selectionStart;
    gtk_text_iter_forward_chars(&replaceEnd, static_cast<int>(g_utf8_strlen(replaceText.c_str(), -1)));
    gtk_text_buffer_select_range(buffer, &selectionStart, &replaceEnd);
    gtk_text_buffer_move_mark(
        buffer,
        gtk_text_buffer_get_mark(buffer, kSearchPositionMark),
        &replaceEnd);

    performFind(state, false);
}

struct ReplaceAllContext {
    AppState* state = nullptr;
    std::string sourceText;
    std::string findText;
    std::string replaceText;
    core::TextReplaceResult result;
    AdwDialog* progressDialog = nullptr;
};

void finishReplaceAllJob(AppState* state) {
    if (state->replaceAllJob == nullptr) {
        return;
    }

    if (state->replaceAllJob->worker.joinable()) {
        state->replaceAllJob->worker.join();
    }

    state->replaceAllJob.reset();
}

void closeReplaceAllDialog(ReplaceAllContext* context) {
    if (context->progressDialog != nullptr) {
        adw_dialog_close(context->progressDialog);
        context->progressDialog = nullptr;
    }
}

gboolean applyReplaceAllOnMainThread(gpointer userData) {
    auto* context = static_cast<ReplaceAllContext*>(userData);
    AppState* state = context->state;

    closeReplaceAllDialog(context);
    finishReplaceAllJob(state);

    if (!context->result.cancelled) {
        if (context->result.limitReached) {
            showToast(state, "Replace All stopped at the safety limit.");
        } else if (context->result.replacements > 0) {
            state->syncingEditor = true;
            GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->textView));
            gtk_text_buffer_set_text(buffer, context->result.text.c_str(), -1);
            state->document->setContent(context->result.text);
            state->syncingEditor = false;
            updateStatus(state);
            showToast(state, "All matches replaced.");
        } else {
            showToast(state, "No matches found.");
        }
    } else {
        showToast(state, "Replace All cancelled.");
    }

    clearSearchHighlight(state);
    delete context;
    return G_SOURCE_REMOVE;
}

void onReplaceAllDialogFinished(GObject* source, GAsyncResult* result, gpointer userData) {
    auto* context = static_cast<ReplaceAllContext*>(userData);
    const char* chosen =
        adw_alert_dialog_choose_finish(ADW_ALERT_DIALOG(source), result);

    if (chosen != nullptr && g_strcmp0(chosen, "cancel") == 0
        && context->state->replaceAllJob != nullptr) {
        context->state->replaceAllJob->cancelRequested.store(true);
    }
}

void startReplaceAllWorker(ReplaceAllContext* context) {
    AppState* state = context->state;
    state->replaceAllJob = std::make_unique<ReplaceAllJob>();
    ReplaceAllJob* job = state->replaceAllJob.get();

    job->worker = std::thread([context, job]() {
        context->result = core::replaceAllInText(
            context->sourceText,
            context->findText,
            context->replaceText,
            core::kMaxReplaceAllReplacements,
            core::kMaxFileSizeBytes,
            &job->cancelRequested);
        g_idle_add(applyReplaceAllOnMainThread, context);
    });
}

void onReplaceAllClicked(GtkButton* /*button*/, gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);
    const std::string findText = getFindText(state);
    const std::string replaceText = getReplaceText(state);

    if (findText.empty()) {
        return;
    }

    if (state->replaceAllJob != nullptr) {
        showToast(state, "Replace All is already running.");
        return;
    }

    syncEditorToDocument(state);
    const std::string& sourceText = state->document->content();
    if (sourceText.size() > core::kMaxFileSizeBytes) {
        showToast(state, "Document exceeds the 50 MB limit.");
        return;
    }

    auto* context = new ReplaceAllContext{
        state,
        sourceText,
        findText,
        replaceText,
        {},
        nullptr,
    };

    AdwDialog* dialog = adw_alert_dialog_new(
        "Replace All",
        "Replacing matches. This may take a moment on large documents.");
    adw_alert_dialog_add_responses(
        ADW_ALERT_DIALOG(dialog),
        "cancel",
        "Cancel",
        nullptr);
    adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "cancel");
    context->progressDialog = dialog;

    adw_alert_dialog_choose(
        ADW_ALERT_DIALOG(dialog),
        GTK_WIDGET(state->window),
        nullptr,
        onReplaceAllDialogFinished,
        context);

    startReplaceAllWorker(context);
}

void onCloseSearchClicked(GtkButton* /*button*/, gpointer userData) {
    hideSearchReplaceBar(static_cast<AppState*>(userData));
}

void onFindEntryActivated(GtkEntry* /*entry*/, gpointer userData) {
    performFind(static_cast<AppState*>(userData), false);
}

void onReplaceEntryActivated(GtkEntry* /*entry*/, gpointer userData) {
    onReplaceClicked(nullptr, userData);
}

gboolean onSearchBarKeyPressed(
    GtkEventControllerKey* /*controller*/,
    guint keyval,
    guint /*keycode*/,
    GdkModifierType modifiers,
    gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);
    const bool shift = (modifiers & GDK_SHIFT_MASK) != 0;

    if (keyval == GDK_KEY_Escape) {
        hideSearchReplaceBar(state);
        return TRUE;
    }

    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
        if (gtk_widget_has_focus(state->replaceEntry)) {
            onReplaceClicked(nullptr, state);
        } else if (shift) {
            performFind(state, true);
        } else {
            performFind(state, false);
        }
        return TRUE;
    }

    return FALSE;
}

GtkWidget* createLabeledEntryRow(
    const char* labelText,
    GtkWidget** entryOut,
    GtkWidget* trailingBox) {
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* label = gtk_label_new(labelText);
    gtk_widget_set_size_request(label, 72, -1);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);

    *entryOut = gtk_entry_new();
    gtk_widget_set_hexpand(*entryOut, TRUE);

    gtk_box_append(GTK_BOX(row), label);
    gtk_box_append(GTK_BOX(row), *entryOut);
    if (trailingBox != nullptr) {
        gtk_box_append(GTK_BOX(row), trailingBox);
    }

    return row;
}

} // namespace

GtkWidget* createSearchReplaceBar(AppState* state) {
    GtkWidget* bar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(bar, "search-replace-bar");
    gtk_widget_set_margin_start(bar, 12);
    gtk_widget_set_margin_end(bar, 12);
    gtk_widget_set_margin_bottom(bar, 4);
    gtk_widget_set_visible(bar, FALSE);
    state->searchBar = bar;

    GtkWidget* findActions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* previousButton = gtk_button_new_with_label("Previous");
    GtkWidget* nextButton = gtk_button_new_with_label("Next");
    GtkWidget* closeButton = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_widget_add_css_class(closeButton, "flat");
    gtk_box_append(GTK_BOX(findActions), previousButton);
    gtk_box_append(GTK_BOX(findActions), nextButton);
    gtk_box_append(GTK_BOX(findActions), closeButton);

    GtkWidget* findRow = createLabeledEntryRow("Find", &state->findEntry, findActions);
    gtk_entry_set_placeholder_text(GTK_ENTRY(state->findEntry), "Search text");

    GtkWidget* replaceActions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* replaceButton = gtk_button_new_with_label("Replace");
    GtkWidget* replaceAllButton = gtk_button_new_with_label("Replace All");
    gtk_box_append(GTK_BOX(replaceActions), replaceButton);
    gtk_box_append(GTK_BOX(replaceActions), replaceAllButton);

    GtkWidget* replaceRow =
        createLabeledEntryRow("Replace", &state->replaceEntry, replaceActions);
    gtk_entry_set_placeholder_text(GTK_ENTRY(state->replaceEntry), "Replace with");

    gtk_box_append(GTK_BOX(bar), findRow);
    gtk_box_append(GTK_BOX(bar), replaceRow);

    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->textView));
    GtkTextIter start;
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_create_mark(buffer, kSearchPositionMark, &start, TRUE);

    g_signal_connect(state->findEntry, "activate", G_CALLBACK(onFindEntryActivated), state);
    g_signal_connect(state->replaceEntry, "activate", G_CALLBACK(onReplaceEntryActivated), state);
    g_signal_connect(previousButton, "clicked", G_CALLBACK(onFindPreviousClicked), state);
    g_signal_connect(nextButton, "clicked", G_CALLBACK(onFindNextClicked), state);
    g_signal_connect(replaceButton, "clicked", G_CALLBACK(onReplaceClicked), state);
    g_signal_connect(replaceAllButton, "clicked", G_CALLBACK(onReplaceAllClicked), state);
    g_signal_connect(closeButton, "clicked", G_CALLBACK(onCloseSearchClicked), state);

    GtkEventController* findKeys = gtk_event_controller_key_new();
    g_signal_connect(findKeys, "key-pressed", G_CALLBACK(onSearchBarKeyPressed), state);
    gtk_widget_add_controller(state->findEntry, findKeys);

    GtkEventController* replaceKeys = gtk_event_controller_key_new();
    g_signal_connect(replaceKeys, "key-pressed", G_CALLBACK(onSearchBarKeyPressed), state);
    gtk_widget_add_controller(state->replaceEntry, replaceKeys);

    return bar;
}

void toggleSearchReplaceBar(AppState* state) {
    if (state->searchBar == nullptr) {
        return;
    }

    if (gtk_widget_get_visible(state->searchBar)) {
        hideSearchReplaceBar(state);
        return;
    }

    showSearchReplaceBar(state);
}

bool isSearchReplaceBarVisible(const AppState* state) {
    return state->searchBar != nullptr && gtk_widget_get_visible(state->searchBar);
}

} // namespace plumas::ui
