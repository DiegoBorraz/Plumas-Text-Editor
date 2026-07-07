#include "plumas/ui/TitleBar.hpp"
#include "plumas/ui/Resources.hpp"

namespace plumas::ui {

namespace {

GdkToplevel* getToplevel(GtkWindow* window) {
    GdkSurface* surface = gtk_native_get_surface(GTK_NATIVE(window));
    if (surface == nullptr) {
        return nullptr;
    }
    return GDK_TOPLEVEL(surface);
}

void updateMaximizeIcon(AppState* state) {
    if (state->maximizeButton == nullptr) {
        return;
    }

    const bool maximized = gtk_window_is_maximized(state->window);
    gtk_button_set_icon_name(
        GTK_BUTTON(state->maximizeButton),
        maximized ? "window-restore-symbolic" : "window-maximize-symbolic");
}

void onMinimizeClicked(GtkButton* /*button*/, gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);
    gtk_window_minimize(state->window);
}

void onMaximizeClicked(GtkButton* /*button*/, gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);

    if (gtk_window_is_maximized(state->window)) {
        gtk_window_unmaximize(state->window);
    } else {
        gtk_window_maximize(state->window);
    }

    updateMaximizeIcon(state);
}

void onCloseClicked(GtkButton* /*button*/, gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);
    gtk_window_close(state->window);
}

void onTitlebarPressed(
    GtkGestureClick* gesture,
    gint /*nPress*/,
    gdouble x,
    gdouble y,
    gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);
    GdkToplevel* toplevel = getToplevel(state->window);
    if (toplevel == nullptr) {
        return;
    }

    GdkEventSequence* sequence =
        gtk_gesture_single_get_current_sequence(GTK_GESTURE_SINGLE(gesture));
    GdkEvent* event = gtk_gesture_get_last_event(GTK_GESTURE(gesture), sequence);
    if (event == nullptr) {
        return;
    }

    gdk_toplevel_begin_move(
        toplevel,
        gdk_event_get_device(event),
        gdk_button_event_get_button(event),
        x,
        y,
        gdk_event_get_time(event));
}

GtkWidget* createTitleLogo(const std::filesystem::path& iconPath) {
    GtkWidget* logo = gtk_image_new_from_file(iconPath.string().c_str());
    gtk_image_set_pixel_size(GTK_IMAGE(logo), 30);
    gtk_widget_set_size_request(logo, 30, 30);
    gtk_widget_set_valign(logo, GTK_ALIGN_CENTER);
    return logo;
}

GtkWidget* createWindowControls(AppState* state) {
    GtkWidget* controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(controls, "window-controls");

    GtkWidget* minimizeButton = gtk_button_new();
    gtk_button_set_icon_name(GTK_BUTTON(minimizeButton), "window-minimize-symbolic");
    gtk_widget_add_css_class(minimizeButton, "flat");
    gtk_widget_set_size_request(minimizeButton, 24, 24);
    gtk_widget_set_tooltip_text(minimizeButton, "Minimize");

    state->maximizeButton = gtk_button_new();
    gtk_button_set_icon_name(GTK_BUTTON(state->maximizeButton), "window-maximize-symbolic");
    gtk_widget_add_css_class(state->maximizeButton, "flat");
    gtk_widget_set_size_request(state->maximizeButton, 24, 24);
    gtk_widget_set_tooltip_text(state->maximizeButton, "Maximize");

    GtkWidget* closeButton = gtk_button_new();
    gtk_button_set_icon_name(GTK_BUTTON(closeButton), "window-close-symbolic");
    gtk_widget_add_css_class(closeButton, "flat");
    gtk_widget_add_css_class(closeButton, "window-close");
    gtk_widget_set_size_request(closeButton, 24, 24);
    gtk_widget_set_tooltip_text(closeButton, "Close");

    g_signal_connect(minimizeButton, "clicked", G_CALLBACK(onMinimizeClicked), state);
    g_signal_connect(state->maximizeButton, "clicked", G_CALLBACK(onMaximizeClicked), state);
    g_signal_connect(closeButton, "clicked", G_CALLBACK(onCloseClicked), state);

    gtk_box_append(GTK_BOX(controls), minimizeButton);
    gtk_box_append(GTK_BOX(controls), state->maximizeButton);
    gtk_box_append(GTK_BOX(controls), closeButton);

    return controls;
}

} // namespace

GtkWidget* createTitleBar(AppState* state) {
    GtkWidget* titleBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(titleBar, "titlebar");
    gtk_widget_set_margin_start(titleBar, 12);
    gtk_widget_set_margin_end(titleBar, 8);
    gtk_widget_set_margin_top(titleBar, 12);

    GtkWidget* titleLabel = gtk_label_new(kWindowTitle);
    gtk_label_set_xalign(GTK_LABEL(titleLabel), 0.0f);
    gtk_widget_add_css_class(titleLabel, "title-2");

    GtkWidget* titleGroup = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    const std::optional<std::filesystem::path> iconPath = findResourcePath(kIconFileName);
    if (iconPath.has_value()) {
        gtk_box_append(GTK_BOX(titleGroup), createTitleLogo(*iconPath));
    }
    gtk_box_append(GTK_BOX(titleGroup), titleLabel);

    state->pathLabel = gtk_label_new("(new document)");
    gtk_label_set_xalign(GTK_LABEL(state->pathLabel), 0.5f);
    gtk_widget_add_css_class(state->pathLabel, "dim-label");
    gtk_label_set_ellipsize(GTK_LABEL(state->pathLabel), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_set_hexpand(state->pathLabel, TRUE);

    GtkWidget* windowControls = createWindowControls(state);
    gtk_widget_set_halign(windowControls, GTK_ALIGN_END);

    gtk_box_append(GTK_BOX(titleBar), titleGroup);
    gtk_box_append(GTK_BOX(titleBar), state->pathLabel);
    gtk_box_append(GTK_BOX(titleBar), windowControls);

    GtkGesture* titleDrag = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(titleDrag), GDK_BUTTON_PRIMARY);
    g_signal_connect(titleDrag, "pressed", G_CALLBACK(onTitlebarPressed), state);
    gtk_widget_add_controller(titleGroup, GTK_EVENT_CONTROLLER(titleDrag));

    return titleBar;
}

} // namespace plumas::ui
