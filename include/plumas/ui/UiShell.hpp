#pragma once

#include "plumas/ui/AppState.hpp"

#include <adwaita.h>
#include <gtk/gtk.h>

namespace plumas::ui {

void initUiToolkit();
GtkWindow* createApplicationWindow(AdwApplication* app);
GtkWidget* createToastOverlay();
void setToastOverlayChild(AppState* state, GtkWidget* child);
void showToast(AppState* state, const char* message);
AdwDialog* createAlertDialog(const char* title, const char* body);

} // namespace plumas::ui
