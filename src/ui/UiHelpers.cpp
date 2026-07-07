#include "plumas/ui/UiHelpers.hpp"

#include "plumas/core/Document.hpp"

#include <adwaita.h>

namespace plumas::ui {

namespace {

void setDarkClass(GtkWidget* widget, bool isDark) {
    if (isDark) {
        gtk_widget_add_css_class(widget, "plumas-dark");
        return;
    }

    gtk_widget_remove_css_class(widget, "plumas-dark");
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

void syncWindowTheme(AppState* state) {
    if (state->window == nullptr) {
        return;
    }

    const bool isDark = adw_style_manager_get_dark(adw_style_manager_get_default());
    setDarkClass(GTK_WIDGET(state->window), isDark);

    if (state->shell != nullptr) {
        setDarkClass(state->shell, isDark);
    }

    if (state->toastOverlay != nullptr) {
        setDarkClass(state->toastOverlay, isDark);
    }
}

} // namespace plumas::ui
