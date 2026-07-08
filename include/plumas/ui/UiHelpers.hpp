#pragma once

#include "plumas/ui/AppState.hpp"

namespace plumas::core {
class Document;
}

namespace plumas::ui {

bool isSaveAvailable(const core::Document& document);
void showToast(AppState* state, const char* message);
void updateStatus(AppState* state);
void setLabelStyle(GtkLabel* label, int fontSizePt, bool bold);
bool beginWindowMoveFromGesture(GtkWindow* window, GtkGestureClick* gesture, double x, double y);
bool beginWindowResizeFromGesture(
    GtkWindow* window,
    GtkGestureClick* gesture,
    GdkSurfaceEdge edge,
    double x,
    double y);
bool isWindowMaximized(GtkWindow* window);
void toggleWindowMaximized(GtkWindow* window);

} // namespace plumas::ui
