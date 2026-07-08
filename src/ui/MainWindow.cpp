#include "plumas/ui/MainWindow.hpp"

#include "plumas/ui/Dialogs.hpp"
#include "plumas/ui/EditorView.hpp"
#include "plumas/ui/SearchReplace.hpp"
#include "plumas/ui/TitleBar.hpp"
#include "plumas/ui/UiHelpers.hpp"

#include <algorithm>

namespace plumas::ui {

namespace {

GdkMonitor* getPrimaryMonitor(GdkDisplay* display) {
    GListModel* monitors = gdk_display_get_monitors(display);
    if (g_list_model_get_n_items(monitors) == 0) {
        return nullptr;
    }
    return GDK_MONITOR(g_list_model_get_item(monitors, 0));
}

void onNewClicked(GtkButton* /*button*/, gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);
    runWithUnsavedCheck(state, newDocument);
}

void onOpenClicked(GtkButton* /*button*/, gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);
    runWithUnsavedCheck(state, actionShowOpenDialog);
}

void onSaveClicked(GtkButton* /*button*/, gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);
    if (isSaveAvailable(*state->document)) {
        saveDocument(state, false);
    }
}

GtkWidget* createToolbar(AppState* state) {
    GtkWidget* toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(toolbar, "editor-toolbar");
    gtk_widget_set_margin_start(toolbar, 12);
    gtk_widget_set_margin_end(toolbar, 12);

    GtkWidget* newButton = gtk_button_new_with_label("New");
    GtkWidget* openButton = gtk_button_new_with_label("Open");
    state->saveButton = gtk_button_new_with_label("Save");

    gtk_widget_set_sensitive(state->saveButton, FALSE);

    g_signal_connect(newButton, "clicked", G_CALLBACK(onNewClicked), state);
    g_signal_connect(openButton, "clicked", G_CALLBACK(onOpenClicked), state);
    g_signal_connect(state->saveButton, "clicked", G_CALLBACK(onSaveClicked), state);

    gtk_box_append(GTK_BOX(toolbar), newButton);
    gtk_box_append(GTK_BOX(toolbar), openButton);
    gtk_box_append(GTK_BOX(toolbar), state->saveButton);

    return toolbar;
}

GtkWidget* createFooter() {
    GtkWidget* footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_margin_start(footer, 20);
    gtk_widget_set_margin_end(footer, 20);
    gtk_widget_set_margin_top(footer, 4);
    gtk_widget_set_margin_bottom(footer, 12);

    GtkWidget* tagline = gtk_label_new(kFooterTagline);
    gtk_label_set_xalign(GTK_LABEL(tagline), 0.5f);
    gtk_widget_add_css_class(tagline, "footer-tagline");
    setLabelStyle(GTK_LABEL(tagline), 14, true);
    gtk_widget_set_hexpand(tagline, TRUE);

    gtk_box_append(GTK_BOX(footer), tagline);
    return footer;
}

gboolean onCloseRequest(GtkWindow* /*window*/, gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);

    if (!state->document->isDirty()) {
        return FALSE;
    }

    auto pending = std::make_unique<PendingAction>(PendingAction{state, closeWindow});
    showUnsavedDialog(state, std::move(pending));
    return TRUE;
}

void onResizePressed(
    GtkGestureClick* gesture,
    gint /*nPress*/,
    gdouble x,
    gdouble y,
    gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);
    const GdkSurfaceEdge edge = static_cast<GdkSurfaceEdge>(GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(gesture), "resize-edge")));
    beginWindowResizeFromGesture(state->window, gesture, edge, x, y);
}

GtkWidget* createResizeHandle(
    AppState* state,
    const GdkSurfaceEdge edge,
    const GtkAlign halign,
    const GtkAlign valign,
    const int width,
    const int height) {
    GtkWidget* handle = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(handle, "resize-handle");
    gtk_widget_set_size_request(handle, width, height);
    gtk_widget_set_halign(handle, halign);
    gtk_widget_set_valign(handle, valign);
    gtk_widget_set_can_target(handle, TRUE);

    GtkGesture* click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), GDK_BUTTON_PRIMARY);
    g_object_set_data(G_OBJECT(click), "resize-edge", GINT_TO_POINTER(edge));
    g_signal_connect(click, "pressed", G_CALLBACK(onResizePressed), state);
    gtk_widget_add_controller(handle, GTK_EVENT_CONTROLLER(click));
    return handle;
}

void addWindowResizeHandles(GtkWidget* overlay, AppState* state) {
    constexpr int kEdgeThickness = 8;
    constexpr int kCornerSize = 14;

    gtk_overlay_add_overlay(
        GTK_OVERLAY(overlay),
        createResizeHandle(
            state, GDK_SURFACE_EDGE_NORTH, GTK_ALIGN_FILL, GTK_ALIGN_START, -1, kEdgeThickness));
    gtk_overlay_add_overlay(
        GTK_OVERLAY(overlay),
        createResizeHandle(
            state, GDK_SURFACE_EDGE_SOUTH, GTK_ALIGN_FILL, GTK_ALIGN_END, -1, kEdgeThickness));
    gtk_overlay_add_overlay(
        GTK_OVERLAY(overlay),
        createResizeHandle(
            state, GDK_SURFACE_EDGE_WEST, GTK_ALIGN_START, GTK_ALIGN_FILL, kEdgeThickness, -1));
    gtk_overlay_add_overlay(
        GTK_OVERLAY(overlay),
        createResizeHandle(
            state, GDK_SURFACE_EDGE_EAST, GTK_ALIGN_END, GTK_ALIGN_FILL, kEdgeThickness, -1));
    gtk_overlay_add_overlay(
        GTK_OVERLAY(overlay),
        createResizeHandle(
            state,
            GDK_SURFACE_EDGE_NORTH_EAST,
            GTK_ALIGN_END,
            GTK_ALIGN_START,
            kCornerSize,
            kCornerSize));
    gtk_overlay_add_overlay(
        GTK_OVERLAY(overlay),
        createResizeHandle(
            state,
            GDK_SURFACE_EDGE_NORTH_WEST,
            GTK_ALIGN_START,
            GTK_ALIGN_START,
            kCornerSize,
            kCornerSize));
    gtk_overlay_add_overlay(
        GTK_OVERLAY(overlay),
        createResizeHandle(
            state,
            GDK_SURFACE_EDGE_SOUTH_EAST,
            GTK_ALIGN_END,
            GTK_ALIGN_END,
            kCornerSize,
            kCornerSize));
    gtk_overlay_add_overlay(
        GTK_OVERLAY(overlay),
        createResizeHandle(
            state,
            GDK_SURFACE_EDGE_SOUTH_WEST,
            GTK_ALIGN_START,
            GTK_ALIGN_END,
            kCornerSize,
            kCornerSize));
}

gboolean onWindowKey(
    GtkEventControllerKey* /*controller*/,
    guint keyval,
    guint /*keycode*/,
    GdkModifierType modifiers,
    gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);
    const bool control = (modifiers & GDK_CONTROL_MASK) != 0;
    const bool shift = (modifiers & GDK_SHIFT_MASK) != 0;

    if (!control) {
        return FALSE;
    }

    if (keyval == GDK_KEY_n || keyval == GDK_KEY_N) {
        runWithUnsavedCheck(state, newDocument);
        return TRUE;
    }

    if (keyval == GDK_KEY_o || keyval == GDK_KEY_O) {
        runWithUnsavedCheck(state, actionShowOpenDialog);
        return TRUE;
    }

    if ((keyval == GDK_KEY_s || keyval == GDK_KEY_S) && shift) {
        saveDocument(state, true);
        return TRUE;
    }

    if (keyval == GDK_KEY_s || keyval == GDK_KEY_S) {
        if (isSaveAvailable(*state->document)) {
            saveDocument(state, false);
        }
        return TRUE;
    }

    if (keyval == GDK_KEY_f || keyval == GDK_KEY_F) {
        toggleSearchReplaceBar(state);
        return TRUE;
    }

    if (keyval == GDK_KEY_q || keyval == GDK_KEY_Q) {
        if (state->document->isDirty()) {
            auto pending = std::make_unique<PendingAction>(PendingAction{state, closeWindow});
            showUnsavedDialog(state, std::move(pending));
        } else {
            closeWindow(state);
        }
        return TRUE;
    }

    return FALSE;
}

} // namespace

void buildMainWindow(AppState* state) {
    GdkDisplay* display = gdk_display_get_default();
    GdkMonitor* monitor = getPrimaryMonitor(display);
    GdkRectangle screen{};
    if (monitor != nullptr) {
        gdk_monitor_get_geometry(monitor, &screen);
        state->monitorMaxWidth = screen.width;
        state->monitorMaxHeight = screen.height;
    }

    const int defaultWidth = monitor != nullptr ? std::min(960, screen.width) : 960;
    const int defaultHeight =
        monitor != nullptr ? std::min(640, std::max(320, screen.height - 48)) : 640;

    state->window = GTK_WINDOW(adw_application_window_new(GTK_APPLICATION(state->app)));
    gtk_window_set_default_size(state->window, defaultWidth, defaultHeight);
    gtk_window_set_resizable(state->window, TRUE);
    gtk_window_set_title(state->window, kWindowTitle);
    gtk_window_set_decorated(state->window, FALSE);
    gtk_widget_add_css_class(GTK_WIDGET(state->window), "plumas-app");

    state->shell = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(state->shell, "plumas-shell");
    gtk_widget_set_vexpand(state->shell, TRUE);
    gtk_widget_set_hexpand(state->shell, TRUE);
    gtk_widget_set_overflow(state->shell, GTK_OVERFLOW_HIDDEN);

    state->toastOverlay = adw_toast_overlay_new();
    gtk_widget_set_vexpand(state->toastOverlay, TRUE);
    gtk_widget_set_hexpand(state->toastOverlay, TRUE);
    gtk_widget_set_overflow(state->toastOverlay, GTK_OVERFLOW_HIDDEN);

    GtkWidget* rootBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(rootBox, "plumas-root");
    gtk_widget_set_vexpand(rootBox, TRUE);
    gtk_widget_set_hexpand(rootBox, TRUE);
    gtk_widget_set_overflow(rootBox, GTK_OVERFLOW_HIDDEN);

    gtk_box_append(GTK_BOX(rootBox), createTitleBar(state));

    GtkWidget* toolbar = createToolbar(state);
    gtk_widget_set_margin_top(toolbar, 8);
    gtk_widget_set_margin_bottom(toolbar, 8);
    gtk_box_append(GTK_BOX(rootBox), toolbar);

    GtkWidget* editorView = createEditorView(state);
    GtkWidget* searchBar = createSearchReplaceBar(state);
    gtk_box_append(GTK_BOX(rootBox), searchBar);

    gtk_widget_set_vexpand(editorView, TRUE);
    gtk_widget_set_hexpand(editorView, TRUE);
    gtk_box_append(GTK_BOX(rootBox), editorView);
    gtk_box_append(GTK_BOX(rootBox), createFooter());

    GtkWidget* windowOverlay = gtk_overlay_new();
    gtk_widget_set_vexpand(windowOverlay, TRUE);
    gtk_widget_set_hexpand(windowOverlay, TRUE);
    gtk_overlay_set_child(GTK_OVERLAY(windowOverlay), rootBox);
    addWindowResizeHandles(windowOverlay, state);

    adw_toast_overlay_set_child(ADW_TOAST_OVERLAY(state->toastOverlay), windowOverlay);
    gtk_box_append(GTK_BOX(state->shell), state->toastOverlay);
    adw_application_window_set_content(
        ADW_APPLICATION_WINDOW(state->window),
        state->shell);

    GtkEventController* windowKeys = gtk_event_controller_key_new();
    g_signal_connect(windowKeys, "key-pressed", G_CALLBACK(onWindowKey), state);
    gtk_widget_add_controller(GTK_WIDGET(state->window), windowKeys);

    g_signal_connect(state->window, "close-request", G_CALLBACK(onCloseRequest), state);

    state->document = std::make_unique<core::Document>();
    refreshEditor(state);
}

void presentMainWindow(AppState* state) {
    gtk_window_present(state->window);
}

} // namespace plumas::ui
