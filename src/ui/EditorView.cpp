#include "plumas/ui/EditorView.hpp"
#include "plumas/ui/UiHelpers.hpp"

#include "plumas/core/FileIO.hpp"

#include <pango/pangocairo.h>

#include <cstring>
#include <string>

namespace plumas::ui {

namespace {

void queueLineGutterRedraw(AppState* state) {
    if (state->lineGutter != nullptr) {
        gtk_widget_queue_draw(state->lineGutter);
    }
}

void drawLineNumbers(
    GtkDrawingArea* /*area*/,
    cairo_t* cr,
    const int width,
    const int height,
    gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);
    if (state->textView == nullptr || state->editorScroll == nullptr) {
        return;
    }

    GtkTextView* textView = GTK_TEXT_VIEW(state->textView);
    GtkAdjustment* adjustment =
        gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(state->editorScroll));
    const double scrollY = gtk_adjustment_get_value(adjustment);

    cairo_set_source_rgb(cr, 0.09, 0.65, 0.66);
    PangoLayout* layout = pango_cairo_create_layout(cr);
    PangoContext* widgetContext = gtk_widget_get_pango_context(GTK_WIDGET(textView));
    pango_layout_set_font_description(
        layout,
        pango_context_get_font_description(widgetContext));

    int firstLineY = 0;
    GtkTextIter firstIter;
    gtk_text_view_get_line_at_y(textView, &firstIter, static_cast<int>(scrollY), &firstLineY);

    int lineY = 0;
    gtk_text_view_get_line_yrange(textView, &firstIter, &lineY, nullptr);
    constexpr int topPadding = 8;
    double y = static_cast<double>(lineY) - scrollY + topPadding;

    GtkTextIter currentIter = firstIter;
    while (y < height) {
        if (gtk_text_iter_is_end(&currentIter)) {
            break;
        }

        int lineHeight = 0;
        gtk_text_view_get_line_yrange(textView, &currentIter, nullptr, &lineHeight);
        if (lineHeight <= 0) {
            break;
        }

        const int lineNumber = gtk_text_iter_get_line(&currentIter) + 1;
        const std::string lineText = std::to_string(lineNumber);
        pango_layout_set_text(layout, lineText.c_str(), -1);
        pango_layout_set_width(layout, (width - 3) * PANGO_SCALE);
        pango_layout_set_alignment(layout, PANGO_ALIGN_RIGHT);

        cairo_move_to(cr, 0, y);
        pango_cairo_show_layout(cr, layout);

        y += lineHeight;
        if (!gtk_text_iter_forward_line(&currentIter)) {
            break;
        }
    }

    g_object_unref(layout);
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
    updateStatus(state);
    queueLineGutterRedraw(state);
}

void onEditorScroll(GtkAdjustment* /*adjustment*/, gpointer userData) {
    queueLineGutterRedraw(static_cast<AppState*>(userData));
}

} // namespace

void refreshEditor(AppState* state) {
    state->syncingEditor = true;

    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->textView));
    gtk_text_buffer_set_text(buffer, state->document->content().c_str(), -1);

    state->syncingEditor = false;
    updateStatus(state);
    queueLineGutterRedraw(state);
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
    state->lineGutter = gtk_drawing_area_new();
    gtk_widget_add_css_class(state->lineGutter, "line-numbers-gutter");
    gtk_widget_set_size_request(state->lineGutter, 24, -1);
    gtk_widget_set_vexpand(state->lineGutter, TRUE);
    gtk_widget_set_hexpand(state->lineGutter, FALSE);
    gtk_drawing_area_set_draw_func(
        GTK_DRAWING_AREA(state->lineGutter),
        drawLineNumbers,
        state,
        nullptr);

    state->editorScroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(state->editorScroll),
        GTK_POLICY_AUTOMATIC,
        GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_propagate_natural_width(GTK_SCROLLED_WINDOW(state->editorScroll), FALSE);
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(state->editorScroll), FALSE);
    gtk_widget_set_vexpand(state->editorScroll, TRUE);
    gtk_widget_set_hexpand(state->editorScroll, TRUE);

    state->textView = gtk_text_view_new();
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(state->textView), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(state->textView), GTK_WRAP_NONE);
    gtk_widget_set_vexpand(state->textView, FALSE);
    gtk_widget_set_hexpand(state->textView, FALSE);
    gtk_widget_add_css_class(state->textView, "editor-main");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(state->editorScroll), state->textView);

    GtkWidget* editorRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(editorRow, "editor-row");
    gtk_widget_set_vexpand(editorRow, TRUE);
    gtk_widget_set_hexpand(editorRow, TRUE);
    gtk_widget_set_overflow(editorRow, GTK_OVERFLOW_HIDDEN);
    gtk_box_append(GTK_BOX(editorRow), state->lineGutter);
    gtk_box_append(GTK_BOX(editorRow), state->editorScroll);

    GtkWidget* editorContainer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(editorContainer, "editor-workspace");
    gtk_widget_set_vexpand(editorContainer, TRUE);
    gtk_widget_set_hexpand(editorContainer, TRUE);
    gtk_widget_set_overflow(editorContainer, GTK_OVERFLOW_HIDDEN);
    gtk_widget_set_margin_start(editorContainer, 20);
    gtk_widget_set_margin_end(editorContainer, 20);
    gtk_widget_set_margin_bottom(editorContainer, 20);
    gtk_box_append(GTK_BOX(editorContainer), editorRow);

    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->textView));
    g_signal_connect(buffer, "insert-text", G_CALLBACK(onInsertText), state);
    g_signal_connect(buffer, "changed", G_CALLBACK(onTextChanged), state);

    GtkAdjustment* editorVadjustment =
        gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(state->editorScroll));
    g_signal_connect(editorVadjustment, "value-changed", G_CALLBACK(onEditorScroll), state);

    return editorContainer;
}

} // namespace plumas::ui
