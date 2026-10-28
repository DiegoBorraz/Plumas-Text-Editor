#include "plumas/ui/MainWindow.hpp"

#include "plumas/ui/Dialogs.hpp"
#include "plumas/ui/EditorView.hpp"
#include "plumas/ui/MenuBar.hpp"
#include "plumas/ui/SearchReplace.hpp"
#include "plumas/ui/Tabs.hpp"
#include "plumas/ui/TitleBar.hpp"
#include "plumas/ui/UiHelpers.hpp"

#include "plumas/platform/Paths.hpp"

#include <algorithm>
#include <optional>

namespace plumas::ui {

namespace {

// Size of the first monitor, or nullopt when there is none yet.
std::optional<GdkRectangle> primaryMonitorGeometry(GdkDisplay* display) {
    GListModel* monitors = gdk_display_get_monitors(display);
    GdkMonitor* monitor = GDK_MONITOR(g_list_model_get_item(monitors, 0));
    if (monitor == nullptr) {
        return std::nullopt;
    }

    GdkRectangle geometry{};
    gdk_monitor_get_geometry(monitor, &geometry);
    g_object_unref(monitor);
    return geometry;
}

GtkWidget* createStatusSegment(GtkWidget* statusBar, const char* text) {
    GtkWidget* separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_append(GTK_BOX(statusBar), separator);

    GtkWidget* label = gtk_label_new(text);
    gtk_widget_add_css_class(label, "status-segment");
    gtk_box_append(GTK_BOX(statusBar), label);
    return label;
}

GtkWidget* createStatusBar(AppState* state) {
    state->statusBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(state->statusBar, "status-bar");

    state->positionLabel = gtk_label_new(nullptr);
    gtk_widget_add_css_class(state->positionLabel, "status-segment");
    gtk_label_set_xalign(GTK_LABEL(state->positionLabel), 0.0f);
    gtk_widget_set_hexpand(state->positionLabel, TRUE);
    gtk_box_append(GTK_BOX(state->statusBar), state->positionLabel);

    state->zoomLabel = createStatusSegment(state->statusBar, nullptr);
    state->lineEndingLabel = createStatusSegment(state->statusBar, nullptr);
    createStatusSegment(state->statusBar, "UTF-8");
    return state->statusBar;
}

gboolean onCloseRequest(GtkWindow* /*window*/, gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);

    if (!hasUnsavedTabs(state)) {
        return FALSE;
    }

    requestQuit(state);
    return TRUE;
}

void onWindowDestroy(GtkWidget* /*window*/, gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);
    // Stops a running Replace All before the widgets it would update are gone.
    state->replaceAllJob.reset();

    // Idle callbacks (tab close, scroll restore) and theme changes can still
    // run after this; with no tabs and null widgets they find nothing to touch.
    state->activeTab = nullptr;
    state->saveTarget = nullptr;
    state->tabs.clear();
    state->textView = nullptr;
    state->lineGutter = nullptr;
    state->editorScroll = nullptr;
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
    constexpr int kEdge = 8;
    constexpr int kCorner = 14;

    struct Handle {
        GdkSurfaceEdge edge;
        GtkAlign halign;
        GtkAlign valign;
        int width;
        int height;
    };
    static constexpr Handle kHandles[] = {
        {GDK_SURFACE_EDGE_NORTH, GTK_ALIGN_FILL, GTK_ALIGN_START, -1, kEdge},
        {GDK_SURFACE_EDGE_SOUTH, GTK_ALIGN_FILL, GTK_ALIGN_END, -1, kEdge},
        {GDK_SURFACE_EDGE_WEST, GTK_ALIGN_START, GTK_ALIGN_FILL, kEdge, -1},
        {GDK_SURFACE_EDGE_EAST, GTK_ALIGN_END, GTK_ALIGN_FILL, kEdge, -1},
        {GDK_SURFACE_EDGE_NORTH_EAST, GTK_ALIGN_END, GTK_ALIGN_START, kCorner, kCorner},
        {GDK_SURFACE_EDGE_NORTH_WEST, GTK_ALIGN_START, GTK_ALIGN_START, kCorner, kCorner},
        {GDK_SURFACE_EDGE_SOUTH_EAST, GTK_ALIGN_END, GTK_ALIGN_END, kCorner, kCorner},
        {GDK_SURFACE_EDGE_SOUTH_WEST, GTK_ALIGN_START, GTK_ALIGN_END, kCorner, kCorner},
    };

    for (const Handle& handle : kHandles) {
        gtk_overlay_add_overlay(
            GTK_OVERLAY(overlay),
            createResizeHandle(
                state, handle.edge, handle.halign, handle.valign, handle.width, handle.height));
    }
}

} // namespace

void buildMainWindow(AppState* state) {
    const std::optional<GdkRectangle> screen = primaryMonitorGeometry(gdk_display_get_default());
    const int defaultWidth = screen.has_value() ? std::min(960, screen->width) : 960;
    const int defaultHeight =
        screen.has_value() ? std::min(640, std::max(320, screen->height - 48)) : 640;

    state->window = GTK_WINDOW(adw_application_window_new(GTK_APPLICATION(state->app)));
    gtk_window_set_default_size(state->window, defaultWidth, defaultHeight);
    gtk_window_set_resizable(state->window, TRUE);
    gtk_window_set_title(state->window, kWindowTitle);
    gtk_window_set_icon_name(state->window, kAppId);
    gtk_window_set_decorated(state->window, platform::usesNativeWindowChrome());
    gtk_widget_add_css_class(GTK_WIDGET(state->window), "plumas-app");

    GtkWidget* shell = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(shell, "plumas-shell");
    gtk_widget_set_vexpand(shell, TRUE);
    gtk_widget_set_hexpand(shell, TRUE);
    gtk_widget_set_overflow(shell, GTK_OVERFLOW_HIDDEN);

    state->toastOverlay = adw_toast_overlay_new();
    gtk_widget_set_vexpand(state->toastOverlay, TRUE);
    gtk_widget_set_hexpand(state->toastOverlay, TRUE);
    gtk_widget_set_overflow(state->toastOverlay, GTK_OVERFLOW_HIDDEN);

    GtkWidget* rootBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(rootBox, "plumas-root");
    gtk_widget_set_vexpand(rootBox, TRUE);
    gtk_widget_set_hexpand(rootBox, TRUE);
    gtk_widget_set_overflow(rootBox, GTK_OVERFLOW_HIDDEN);

    installWindowActions(state);
    gtk_box_append(GTK_BOX(rootBox), createTitleBar(state));

    GtkWidget* editorView = createEditorView(state);
    GtkWidget* searchBar = createSearchReplaceBar(state);
    gtk_box_append(GTK_BOX(rootBox), searchBar);

    gtk_widget_set_vexpand(editorView, TRUE);
    gtk_widget_set_hexpand(editorView, TRUE);
    gtk_box_append(GTK_BOX(rootBox), editorView);
    gtk_box_append(GTK_BOX(rootBox), createStatusBar(state));

    GtkWidget* windowOverlay = gtk_overlay_new();
    gtk_widget_set_vexpand(windowOverlay, TRUE);
    gtk_widget_set_hexpand(windowOverlay, TRUE);
    gtk_overlay_set_child(GTK_OVERLAY(windowOverlay), rootBox);
    if (!platform::usesNativeWindowChrome()) {
        addWindowResizeHandles(windowOverlay, state);
    }

    adw_toast_overlay_set_child(ADW_TOAST_OVERLAY(state->toastOverlay), windowOverlay);
    gtk_box_append(GTK_BOX(shell), state->toastOverlay);
    adw_application_window_set_content(ADW_APPLICATION_WINDOW(state->window), shell);

    g_signal_connect(state->window, "close-request", G_CALLBACK(onCloseRequest), state);
    g_signal_connect(state->window, "destroy", G_CALLBACK(onWindowDestroy), state);

    applyEditorColors(state);
    setEditorZoom(state, kDefaultZoomPercent);
    newTab(state);
}

void presentMainWindow(AppState* state) {
    gtk_window_present(state->window);
}

} // namespace plumas::ui
