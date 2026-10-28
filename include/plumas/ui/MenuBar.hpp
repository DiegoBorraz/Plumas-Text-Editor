#pragma once

#include "plumas/ui/AppState.hpp"

namespace plumas::ui {

// Registers the win.* actions and their keyboard shortcuts.
void installWindowActions(AppState* state);
GtkWidget* createMenuBar();

} // namespace plumas::ui
