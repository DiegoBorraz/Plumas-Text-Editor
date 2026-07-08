#include "plumas/ui/UiHelpers.hpp"

#include "plumas/core/Document.hpp"

#include <adwaita.h>
#include <pango/pango.h>

#include <algorithm>

namespace plumas::ui {

namespace {

bool toSurfaceCoordinates(
    GtkWidget* widget,
    GtkWindow* window,
    const double localX,
    const double localY,
    double* surfaceX,
    double* surfaceY) {
    GtkNative* native = GTK_NATIVE(window);
    if (native == nullptr) {
        return false;
    }

    graphene_point_t localPoint = {};
    graphene_point_t surfacePoint = {};
    graphene_point_init(&localPoint, static_cast<float>(localX), static_cast<float>(localY));

    if (!gtk_widget_compute_point(widget, GTK_WIDGET(native), &localPoint, &surfacePoint)) {
        return false;
    }

    *surfaceX = surfacePoint.x;
    *surfaceY = surfacePoint.y;
    return true;
}

GdkToplevel* getWindowToplevel(GtkWindow* window) {
    GdkSurface* surface = gtk_native_get_surface(GTK_NATIVE(window));
    if (surface == nullptr || !GDK_IS_TOPLEVEL(surface)) {
        return nullptr;
    }
    return GDK_TOPLEVEL(surface);
}

} // namespace

bool isSaveAvailable(const core::Document& document) {
    return document.isDirty() || document.filePath().empty();
}

void showToast(AppState* state, const char* message) {
    AdwToast* toast = adw_toast_new(message);
    adw_toast_overlay_add_toast(ADW_TOAST_OVERLAY(state->toastOverlay), toast);
}

void updateStatus(AppState* state) {
    const std::string& path = state->document->filePath();

    std::string pathText;
    if (path.empty()) {
        pathText = "(new document)";
    } else {
        pathText = path;
    }

    gtk_label_set_text(GTK_LABEL(state->pathLabel), pathText.c_str());
    gtk_window_set_title(state->window, kWindowTitle);
    gtk_widget_set_sensitive(state->saveButton, isSaveAvailable(*state->document));
}

void setLabelStyle(GtkLabel* label, const int fontSizePt, const bool bold) {
    PangoAttrList* attributes = pango_attr_list_new();

    if (bold) {
        pango_attr_list_insert(attributes, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    }

    if (fontSizePt > 0) {
        pango_attr_list_insert(
            attributes,
            pango_attr_size_new(fontSizePt * PANGO_SCALE));
    }

    gtk_label_set_attributes(label, attributes);
    pango_attr_list_unref(attributes);
}

bool beginWindowMoveFromGesture(
    GtkWindow* window,
    GtkGestureClick* gesture,
    const double x,
    const double y) {
    GdkSurface* surface = gtk_native_get_surface(GTK_NATIVE(window));
    if (surface == nullptr || !GDK_IS_TOPLEVEL(surface)) {
        return false;
    }

    GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    double surfaceX = x;
    double surfaceY = y;
    if (!toSurfaceCoordinates(widget, window, x, y, &surfaceX, &surfaceY)) {
        return false;
    }

    GdkEventSequence* sequence =
        gtk_gesture_single_get_current_sequence(GTK_GESTURE_SINGLE(gesture));
    GdkEvent* event = gtk_gesture_get_last_event(GTK_GESTURE(gesture), sequence);
    if (event == nullptr) {
        return false;
    }

    gdk_toplevel_begin_move(
        GDK_TOPLEVEL(surface),
        gdk_event_get_device(event),
        gdk_button_event_get_button(event),
        surfaceX,
        surfaceY,
        gdk_event_get_time(event));
    return true;
}

bool beginWindowResizeFromGesture(
    GtkWindow* window,
    GtkGestureClick* gesture,
    const GdkSurfaceEdge edge,
    const double x,
    const double y) {
    GdkSurface* surface = gtk_native_get_surface(GTK_NATIVE(window));
    if (surface == nullptr || !GDK_IS_TOPLEVEL(surface)) {
        return false;
    }

    GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    double surfaceX = x;
    double surfaceY = y;
    if (!toSurfaceCoordinates(widget, window, x, y, &surfaceX, &surfaceY)) {
        return false;
    }

    GdkEventSequence* sequence =
        gtk_gesture_single_get_current_sequence(GTK_GESTURE_SINGLE(gesture));
    GdkEvent* event = gtk_gesture_get_last_event(GTK_GESTURE(gesture), sequence);
    if (event == nullptr) {
        return false;
    }

    gdk_toplevel_begin_resize(
        GDK_TOPLEVEL(surface),
        edge,
        gdk_event_get_device(event),
        gdk_button_event_get_button(event),
        surfaceX,
        surfaceY,
        gdk_event_get_time(event));
    return true;
}

bool isWindowMaximized(GtkWindow* window) {
    GdkToplevel* toplevel = getWindowToplevel(window);
    if (toplevel == nullptr) {
        return false;
    }

    return (gdk_toplevel_get_state(toplevel) & GDK_TOPLEVEL_STATE_MAXIMIZED) != 0;
}

void toggleWindowMaximized(GtkWindow* window) {
    GdkToplevel* toplevel = getWindowToplevel(window);
    if (toplevel == nullptr) {
        return;
    }

    GdkToplevelLayout* layout = gdk_toplevel_layout_new();
    gdk_toplevel_layout_set_maximized(layout, !isWindowMaximized(window));
    gdk_toplevel_layout_set_resizable(layout, TRUE);
    gdk_toplevel_present(toplevel, layout);
    gdk_toplevel_layout_unref(layout);
}

} // namespace plumas::ui
