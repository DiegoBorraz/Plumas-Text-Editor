#include "plumas/ui/TitleBar.hpp"
#include "plumas/ui/Resources.hpp"
#include "plumas/ui/UiHelpers.hpp"

#include "plumas/core/FileIO.hpp"
#include "plumas/platform/Paths.hpp"

#include <optional>
#include <string>

namespace plumas::ui {

namespace {

void updateMaximizeIcon(AppState* state) {
    if (state->maximizeButton == nullptr) {
        return;
    }

    const bool maximized = isWindowMaximized(state->window);
    gtk_button_set_icon_name(
        GTK_BUTTON(state->maximizeButton),
        maximized ? "window-restore-symbolic" : "window-maximize-symbolic");
}

void onMinimizeClicked(GtkButton* /*button*/, gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);
    gtk_window_minimize(state->window);
}

void scheduleMaximizeIconUpdate(AppState* state) {
    g_idle_add(
        +[](gpointer userData) -> gboolean {
            updateMaximizeIcon(static_cast<AppState*>(userData));
            return G_SOURCE_REMOVE;
        },
        state);
}

void onMaximizeClicked(GtkButton* /*button*/, gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);
    toggleWindowMaximized(state->window);
    scheduleMaximizeIconUpdate(state);
}

void onCloseClicked(GtkButton* /*button*/, gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);
    gtk_window_close(state->window);
}

void onTitlebarPressed(
    GtkGestureClick* gesture,
    gint nPress,
    gdouble x,
    gdouble y,
    gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);

    if (nPress == 2) {
        toggleWindowMaximized(state->window);
        scheduleMaximizeIconUpdate(state);
        return;
    }

    beginWindowMoveFromGesture(state->window, gesture, x, y);
}

void attachTitleBarDrag(GtkWidget* widget, AppState* state) {
    GtkGesture* titleDrag = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(titleDrag), GDK_BUTTON_PRIMARY);
    g_signal_connect(titleDrag, "pressed", G_CALLBACK(onTitlebarPressed), state);
    gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(titleDrag));
}

GtkWidget* createTitleLogoFromResource() {
    GtkWidget* logo = gtk_image_new_from_resource(resourcePath(kIconFileName).c_str());
    gtk_image_set_pixel_size(GTK_IMAGE(logo), 30);
    gtk_widget_set_size_request(logo, 30, 30);
    gtk_widget_set_valign(logo, GTK_ALIGN_CENTER);
    return logo;
}

GtkWidget* createTitleLogoFromFile(const std::string& iconPath) {
    GtkWidget* logo = gtk_image_new_from_file(iconPath.c_str());
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
    gtk_widget_add_css_class(titleLabel, "app-title");
    setLabelStyle(GTK_LABEL(titleLabel), kAppTitleFontSizePt, true);

    GtkWidget* titleGroup = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    if (hasBundledResource(kIconFileName)) {
        gtk_box_append(GTK_BOX(titleGroup), createTitleLogoFromResource());
    } else if (isSafeFilesystemImagePath(kIconFileName, core::kMaxIconFileSizeBytes)) {
        const std::optional<std::string> iconPath = findFilesystemResourcePath(kIconFileName);
        if (iconPath.has_value()) {
            gtk_box_append(GTK_BOX(titleGroup), createTitleLogoFromFile(*iconPath));
        }
    }
    gtk_box_append(GTK_BOX(titleGroup), titleLabel);

    state->pathLabel = gtk_label_new("(new document)");
    gtk_label_set_xalign(GTK_LABEL(state->pathLabel), 0.5f);
    gtk_widget_add_css_class(state->pathLabel, "document-path");
    gtk_label_set_ellipsize(GTK_LABEL(state->pathLabel), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_set_hexpand(state->pathLabel, TRUE);

    gtk_box_append(GTK_BOX(titleBar), titleGroup);
    gtk_box_append(GTK_BOX(titleBar), state->pathLabel);

    if (!platform::usesNativeWindowChrome()) {
        GtkWidget* windowControls = createWindowControls(state);
        gtk_widget_set_halign(windowControls, GTK_ALIGN_END);
        gtk_box_append(GTK_BOX(titleBar), windowControls);

        attachTitleBarDrag(titleGroup, state);
        attachTitleBarDrag(state->pathLabel, state);
    }

    return titleBar;
}

} // namespace plumas::ui
