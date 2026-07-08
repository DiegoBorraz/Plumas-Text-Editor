#include "plumas/ui/UiShell.hpp"

#include <adwaita.h>

namespace plumas::ui {

void initUiToolkit() {
    adw_init();
}

GtkWindow* createApplicationWindow(AdwApplication* app) {
    return GTK_WINDOW(adw_application_window_new(GTK_APPLICATION(app)));
}

GtkWidget* createToastOverlay() {
    GtkWidget* overlay = adw_toast_overlay_new();
    gtk_widget_set_vexpand(overlay, TRUE);
    gtk_widget_set_hexpand(overlay, TRUE);
    gtk_widget_set_overflow(overlay, GTK_OVERFLOW_HIDDEN);
    return overlay;
}

void setToastOverlayChild(AppState* state, GtkWidget* child) {
    adw_toast_overlay_set_child(ADW_TOAST_OVERLAY(state->toastOverlay), child);
}

void showToast(AppState* state, const char* message) {
    AdwToast* toast = adw_toast_new(message);
    adw_toast_overlay_add_toast(ADW_TOAST_OVERLAY(state->toastOverlay), toast);
}

AdwDialog* createAlertDialog(const char* title, const char* body) {
    return ADW_DIALOG(adw_alert_dialog_new(title, body));
}

} // namespace plumas::ui
