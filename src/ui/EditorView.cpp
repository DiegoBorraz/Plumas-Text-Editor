#include "plumas/ui/EditorView.hpp"
#include "plumas/ui/UiHelpers.hpp"

#include "plumas/core/FileIO.hpp"

#include <cstring>
#include <string>

namespace plumas::ui {

namespace {

void updateLineNumbers(AppState* state) {
    GtkTextBuffer* editorBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->textView));
    const int lineCount = gtk_text_buffer_get_line_count(editorBuffer);

    if (lineCount == state->lastLineCount) {
        return;
    }

    state->lastLineCount = lineCount;

    std::string numbers;
    numbers.reserve(static_cast<std::size_t>(lineCount) * 4);
    for (int line = 1; line <= lineCount; ++line) {
        numbers += std::to_string(line);
        if (line < lineCount) {
            numbers += '\n';
        }
    }

    GtkTextBuffer* gutterBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->lineNumberView));
    gtk_text_buffer_set_text(gutterBuffer, numbers.c_str(), -1);
}

void onInsertText(
    GtkTextBuffer* buffer,
    const GtkTextIter* /*position*/,
    const char* text,
    int len,
    gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);
    if (state->syncingEditor) {
        return;
    }

    const int currentChars = gtk_text_buffer_get_char_count(buffer);
    const glong incomingChars = g_utf8_strlen(text, len);
    if (static_cast<gsize>(currentChars) + static_cast<gsize>(incomingChars) > core::kMaxEditorChars) {
        showToast(state, "50 MB limit reached.");
        g_signal_stop_emission_by_name(buffer, "insert-text");
    }
}

void onTextChanged(GtkTextBuffer* /*buffer*/, gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);
    if (state->syncingEditor) {
        return;
    }

    state->document->markDirty();
    updateLineNumbers(state);
    updateStatus(state);
}

void onEditorScroll(GtkAdjustment* adjustment, gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);
    if (state->syncingScroll) {
        return;
    }

    GtkWidget* lineScroll = static_cast<GtkWidget*>(
        g_object_get_data(G_OBJECT(state->lineNumberView), "line-scroll"));
    if (lineScroll == nullptr) {
        return;
    }

    GtkAdjustment* lineAdjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(lineScroll));
    state->syncingScroll = true;
    gtk_adjustment_set_value(lineAdjustment, gtk_adjustment_get_value(adjustment));
    state->syncingScroll = false;
}

} // namespace

void refreshEditor(AppState* state) {
    state->syncingEditor = true;

    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->textView));
    gtk_text_buffer_set_text(buffer, state->document->content().c_str(), -1);

    state->syncingEditor = false;
    state->lastLineCount = 0;
    updateLineNumbers(state);
    updateStatus(state);
}

void syncEditorToDocument(AppState* state) {
    GtkTextBuffer* editorBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->textView));
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(editorBuffer, &start, &end);
    char* text = gtk_text_buffer_get_text(editorBuffer, &start, &end, FALSE);
    state->document->setContent(text != nullptr ? text : "");
    g_free(text);
}

bool isEditorWithinSizeLimit(AppState* state) {
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->textView));
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    char* text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    const gsize byteCount = text != nullptr ? std::strlen(text) : 0;
    g_free(text);

    if (byteCount > core::kMaxFileSizeBytes) {
        showToast(state, "Document exceeds the 50 MB limit.");
        return false;
    }
    return true;
}

GtkWidget* createEditorView(AppState* state) {
    GtkWidget* editorPaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);

    GtkWidget* lineScroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(lineScroll),
        GTK_POLICY_NEVER,
        GTK_POLICY_NEVER);
    gtk_widget_set_size_request(lineScroll, 24, -1);
    gtk_widget_set_vexpand(lineScroll, TRUE);

    state->lineNumberView = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(state->lineNumberView), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(state->lineNumberView), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(state->lineNumberView), TRUE);
    gtk_text_view_set_justification(GTK_TEXT_VIEW(state->lineNumberView), GTK_JUSTIFY_RIGHT);
    gtk_widget_add_css_class(state->lineNumberView, "line-numbers");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(lineScroll), state->lineNumberView);
    g_object_set_data(G_OBJECT(state->lineNumberView), "line-scroll", lineScroll);

    GtkWidget* editorScroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(editorScroll),
        GTK_POLICY_AUTOMATIC,
        GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(editorScroll, TRUE);
    gtk_widget_set_hexpand(editorScroll, TRUE);

    state->textView = gtk_text_view_new();
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(state->textView), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(state->textView), GTK_WRAP_NONE);
    gtk_widget_add_css_class(state->textView, "editor-main");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(editorScroll), state->textView);

    gtk_paned_set_start_child(GTK_PANED(editorPaned), lineScroll);
    gtk_paned_set_shrink_start_child(GTK_PANED(editorPaned), FALSE);
    gtk_paned_set_resize_start_child(GTK_PANED(editorPaned), FALSE);
    gtk_paned_set_end_child(GTK_PANED(editorPaned), editorScroll);
    gtk_paned_set_shrink_end_child(GTK_PANED(editorPaned), TRUE);
    gtk_paned_set_resize_end_child(GTK_PANED(editorPaned), TRUE);

    GtkWidget* editorContainer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(editorContainer, TRUE);
    gtk_widget_set_hexpand(editorContainer, TRUE);
    gtk_widget_set_overflow(editorContainer, GTK_OVERFLOW_HIDDEN);
    gtk_widget_set_margin_start(editorContainer, 20);
    gtk_widget_set_margin_end(editorContainer, 20);
    gtk_widget_set_margin_bottom(editorContainer, 20);
    gtk_box_append(GTK_BOX(editorContainer), editorPaned);

    gtk_widget_set_vexpand(editorPaned, TRUE);
    gtk_widget_set_hexpand(editorPaned, TRUE);
    gtk_widget_set_overflow(editorPaned, GTK_OVERFLOW_HIDDEN);

    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->textView));
    g_signal_connect(buffer, "insert-text", G_CALLBACK(onInsertText), state);
    g_signal_connect(buffer, "changed", G_CALLBACK(onTextChanged), state);

    GtkAdjustment* editorVadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(editorScroll));
    g_signal_connect(editorVadjustment, "value-changed", G_CALLBACK(onEditorScroll), state);

    return editorContainer;
}

} // namespace plumas::ui
