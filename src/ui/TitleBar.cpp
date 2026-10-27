#include "plumas/ui/TitleBar.hpp"
#include "plumas/ui/MenuBar.hpp"
#include "plumas/ui/Resources.hpp"
#include "plumas/ui/Tabs.hpp"
#include "plumas/ui/UiHelpers.hpp"

#include "plumas/core/FileIO.hpp"
#include "plumas/platform/Paths.hpp"

#include <optional>
#include <string>

namespace plumas::ui {

namespace {

constexpr int kTitleIconSize = 16;

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
    gtk_image_set_pixel_size(GTK_IMAGE(logo), kTitleIconSize);
    gtk_widget_set_valign(logo, GTK_ALIGN_CENTER);
    return logo;
}

GtkWidget* createTitleLogoFromFile(const std::string& iconPath) {
    GtkWidget* logo = gtk_image_new_from_file(iconPath.c_str());
    gtk_image_set_pixel_size(GTK_IMAGE(logo), kTitleIconSize);
    gtk_widget_set_valign(logo, GTK_ALIGN_CENTER);
    return logo;
}

GtkWidget* createWindowControls(AppState* state) {
    GtkWidget* controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(controls, "window-controls");

    GtkWidget* minimizeButton = gtk_button_new();
    gtk_button_set_icon_name(GTK_BUTTON(minimizeButton), "window-minimize-symbolic");
    gtk_widget_add_css_class(minimizeButton, "flat");
    gtk_widget_set_tooltip_text(minimizeButton, "Minimize");

    state->maximizeButton = gtk_button_new();
    gtk_button_set_icon_name(GTK_BUTTON(state->maximizeButton), "window-maximize-symbolic");
    gtk_widget_add_css_class(state->maximizeButton, "flat");
    gtk_widget_set_tooltip_text(state->maximizeButton, "Maximize");

    GtkWidget* closeButton = gtk_button_new();
    gtk_button_set_icon_name(GTK_BUTTON(closeButton), "window-close-symbolic");
    gtk_widget_add_css_class(closeButton, "flat");
    gtk_widget_add_css_class(closeButton, "window-close");
    gtk_widget_set_tooltip_text(closeButton, "Close");

    g_signal_connect(minimizeButton, "clicked", G_CALLBACK(onMinimizeClicked), state);
    g_signal_connect(state->maximizeButton, "clicked", G_CALLBACK(onMaximizeClicked), state);
    g_signal_connect(closeButton, "clicked", G_CALLBACK(onCloseClicked), state);

    gtk_box_append(GTK_BOX(controls), minimizeButton);
    gtk_box_append(GTK_BOX(controls), state->maximizeButton);
    gtk_box_append(GTK_BOX(controls), closeButton);

    return controls;
}

GtkWidget* createLogo() {
    if (hasBundledResource(kIconFileName)) {
        return createTitleLogoFromResource();
    }
    if (const std::optional<std::string> iconPath =
            findSafeFilesystemImagePath(kIconFileName, core::kMaxIconFileSizeBytes)) {
        return createTitleLogoFromFile(*iconPath);
    }
    return nullptr;
}

// Win11 Notepad layout: icon, tabs and "+", empty drag space, window buttons.
GtkWidget* createTabRow(AppState* state) {
    const bool customChrome = !platform::usesNativeWindowChrome();

    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(row, "titlebar");

    if (customChrome) {
        if (GtkWidget* logo = createLogo()) {
            gtk_widget_add_css_class(logo, "title-logo");
            gtk_box_append(GTK_BOX(row), logo);
        }
    }

    gtk_box_append(GTK_BOX(row), createTabStrip(state));

    GtkWidget* dragArea = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(dragArea, TRUE);
    gtk_box_append(GTK_BOX(row), dragArea);

    if (customChrome) {
        attachTitleBarDrag(dragArea, state);
        gtk_box_append(GTK_BOX(row), createWindowControls(state));
    }
    return row;
}

} // namespace

GtkWidget* createTitleBar(AppState* state) {
    GtkWidget* chrome = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(chrome, "window-chrome");

    gtk_box_append(GTK_BOX(chrome), createTabRow(state));
    gtk_box_append(GTK_BOX(chrome), createMenuBar());
    return chrome;
}

} // namespace plumas::ui
