#include "plumas/ui/EditorView.hpp"
#include "plumas/ui/UiHelpers.hpp"

#include "plumas/core/FileIO.hpp"

#include <pango/pangocairo.h>

#include <algorithm>
#include <string>
#include <string_view>

namespace plumas::ui {

namespace {

constexpr int kLineGutterPaddingTop = 4;
constexpr int kLineGutterPaddingLeft = 12;
constexpr int kLineGutterPaddingRight = 8;
constexpr std::size_t kMinLineNumberDigits = 2;
constexpr int kEditorPadding = 4;
constexpr guint kMaxUndoLevels = 100;
// Notepad's default editor font size, in tenths of a point.
constexpr int kBaseFontSizeTenthsPt = 110;
constexpr int kMinZoomPercent = 10;
constexpr int kMaxZoomPercent = 500;
constexpr int kScrollZoomStepPercent = 10;

void queueLineGutterRedraw(AppState* state) {
    if (state->lineGutter != nullptr) {
        gtk_widget_queue_draw(state->lineGutter);
    }
}

// Sizes the gutter for the widest line number in the current font, so it
// follows both the line count and the zoom. Resizing queues one more frame.
void fitLineGutterWidth(AppState* state, PangoLayout* layout) {
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->textView));
    const std::size_t digits = std::max<std::size_t>(
        kMinLineNumberDigits,
        std::to_string(gtk_text_buffer_get_line_count(buffer)).size());

    pango_layout_set_text(layout, std::string(digits, '0').c_str(), -1);
    int textWidth = 0;
    pango_layout_get_pixel_size(layout, &textWidth, nullptr);
    const int wanted = textWidth + kLineGutterPaddingLeft + kLineGutterPaddingRight;

    int current = 0;
    gtk_widget_get_size_request(state->lineGutter, &current, nullptr);
    if (current != wanted) {
        gtk_widget_set_size_request(state->lineGutter, wanted, -1);
    }
}

void drawLineNumbers(
    GtkDrawingArea* area,
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

    // The gutter's colour comes from styles.css like every other widget.
    GdkRGBA lineNumberColor;
    gtk_widget_get_color(GTK_WIDGET(area), &lineNumberColor);
    gdk_cairo_set_source_rgba(cr, &lineNumberColor);

    PangoLayout* layout = pango_cairo_create_layout(cr);
    // Subpixel antialiasing tints thin grey digits orange and blue.
    cairo_font_options_t* fontOptions = cairo_font_options_create();
    cairo_font_options_set_antialias(fontOptions, CAIRO_ANTIALIAS_GRAY);
    pango_cairo_context_set_font_options(pango_layout_get_context(layout), fontOptions);
    cairo_font_options_destroy(fontOptions);
    pango_layout_context_changed(layout);

    PangoContext* widgetContext = gtk_widget_get_pango_context(GTK_WIDGET(textView));
    pango_layout_set_font_description(
        layout,
        pango_context_get_font_description(widgetContext));

    fitLineGutterWidth(state, layout);

    int firstLineY = 0;
    GtkTextIter firstIter;
    gtk_text_view_get_line_at_y(textView, &firstIter, static_cast<int>(scrollY), &firstLineY);

    int lineY = 0;
    gtk_text_view_get_line_yrange(textView, &firstIter, &lineY, nullptr);
    double y = static_cast<double>(lineY) - scrollY + kLineGutterPaddingTop;

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
        pango_layout_set_width(layout, (width - kLineGutterPaddingRight) * PANGO_SCALE);
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

    state->activeTab->document->markDirty();
    updateStatus(state);
    queueLineGutterRedraw(state);
}

void onEditorScroll(GtkAdjustment* /*adjustment*/, gpointer userData) {
    queueLineGutterRedraw(static_cast<AppState*>(userData));
}

void onMarkSet(
    GtkTextBuffer* buffer,
    const GtkTextIter* /*location*/,
    GtkTextMark* mark,
    gpointer userData) {
    if (mark == gtk_text_buffer_get_insert(buffer)) {
        updateStatus(static_cast<AppState*>(userData));
    }
}

// Ctrl+wheel zooms, as in Notepad.
gboolean onEditorWheel(
    GtkEventControllerScroll* controller,
    const double /*dx*/,
    const double dy,
    gpointer userData) {
    const GdkModifierType modifiers =
        gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(controller));
    if ((modifiers & GDK_CONTROL_MASK) == 0 || dy == 0.0) {
        return FALSE;
    }

    AppState* state = static_cast<AppState*>(userData);
    setEditorZoom(
        state,
        state->zoomPercent + (dy < 0.0 ? kScrollZoomStepPercent : -kScrollZoomStepPercent));
    return TRUE;
}

} // namespace

void setEditorText(AppState* state, const std::string_view text) {
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->textView));

    // Irreversible so the undo history (and the memory it holds) starts empty.
    state->syncingEditor = true;
    gtk_text_buffer_begin_irreversible_action(buffer);
    gtk_text_buffer_set_text(buffer, text.data(), static_cast<int>(text.size()));
    gtk_text_buffer_end_irreversible_action(buffer);
    // Like Notepad, a freshly loaded text starts with the cursor at the top.
    GtkTextIter start;
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_place_cursor(buffer, &start);
    // The view may still hold the previous buffer's scroll offset. Resetting
    // the adjustments directly avoids scroll_to_mark() before allocation,
    // which makes GTK warn about snapshotting unallocated widgets.
    GtkScrolledWindow* scroll = GTK_SCROLLED_WINDOW(state->editorScroll);
    gtk_adjustment_set_value(gtk_scrolled_window_get_vadjustment(scroll), 0.0);
    gtk_adjustment_set_value(gtk_scrolled_window_get_hadjustment(scroll), 0.0);
    state->syncingEditor = false;

    updateStatus(state);
    queueLineGutterRedraw(state);
}

void setEditorZoom(AppState* state, const int percent) {
    state->zoomPercent = std::clamp(percent, kMinZoomPercent, kMaxZoomPercent);

    // Integer maths: printf("%f") would write a decimal comma in some locales,
    // which CSS rejects.
    const int tenths = kBaseFontSizeTenthsPt * state->zoomPercent / 100;
    const std::string css = "textview.editor-text { font-size: " + std::to_string(tenths / 10)
        + "." + std::to_string(tenths % 10) + "pt; }";

    if (state->zoomProvider == nullptr) {
        state->zoomProvider = gtk_css_provider_new();
        gtk_style_context_add_provider_for_display(
            gdk_display_get_default(),
            GTK_STYLE_PROVIDER(state->zoomProvider),
            GTK_STYLE_PROVIDER_PRIORITY_USER + 1);
    }
    gtk_css_provider_load_from_string(state->zoomProvider, css.c_str());

    updateStatus(state);
    queueLineGutterRedraw(state);
}

GCharPtr copyEditorText(AppState* state) {
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->textView));
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    return GCharPtr(gtk_text_buffer_get_text(buffer, &start, &end, FALSE), g_free);
}

void attachEditorBuffer(AppState* state, GtkTextBuffer* buffer) {
    // Unlimited undo would keep every edit of a long session in memory.
    gtk_text_buffer_set_max_undo_levels(buffer, kMaxUndoLevels);
    g_signal_connect(buffer, "insert-text", G_CALLBACK(onInsertText), state);
    g_signal_connect(buffer, "changed", G_CALLBACK(onTextChanged), state);
    g_signal_connect(buffer, "mark-set", G_CALLBACK(onMarkSet), state);
}

GtkWidget* createEditorView(AppState* state) {
    state->lineGutter = gtk_drawing_area_new();
    gtk_widget_add_css_class(state->lineGutter, "line-gutter");
    gtk_widget_set_vexpand(state->lineGutter, TRUE);
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
    gtk_widget_set_vexpand(state->editorScroll, TRUE);
    gtk_widget_set_hexpand(state->editorScroll, TRUE);

    state->textView = gtk_text_view_new();
    gtk_widget_add_css_class(state->textView, "editor-text");
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(state->textView), GTK_WRAP_NONE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(state->textView), kEditorPadding);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(state->textView), kEditorPadding);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(state->textView), kEditorPadding);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(state->textView), kEditorPadding);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(state->editorScroll), state->textView);

    GtkEventController* wheel =
        gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    gtk_event_controller_set_propagation_phase(wheel, GTK_PHASE_CAPTURE);
    g_signal_connect(wheel, "scroll", G_CALLBACK(onEditorWheel), state);
    gtk_widget_add_controller(state->editorScroll, wheel);

    GtkWidget* editorRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(editorRow, "editor-row");
    gtk_widget_set_vexpand(editorRow, TRUE);
    gtk_widget_set_hexpand(editorRow, TRUE);
    gtk_box_append(GTK_BOX(editorRow), state->lineGutter);
    gtk_box_append(GTK_BOX(editorRow), state->editorScroll);

    GtkAdjustment* editorVadjustment =
        gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(state->editorScroll));
    g_signal_connect(editorVadjustment, "value-changed", G_CALLBACK(onEditorScroll), state);
    // Fires once a newly shown tab's buffer has been laid out, so the gutter
    // is not left blank after a tab switch.
    g_signal_connect(editorVadjustment, "changed", G_CALLBACK(onEditorScroll), state);

    return editorRow;
}

} // namespace plumas::ui
