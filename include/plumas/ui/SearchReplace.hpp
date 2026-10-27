#pragma once

#include "plumas/ui/AppState.hpp"

namespace plumas::ui {

GtkWidget* createSearchReplaceBar(AppState* state);
void toggleSearchReplaceBar(AppState* state);
// Drops a running Replace All so its result is never applied to another document.
void abandonReplaceAll(AppState* state);

} // namespace plumas::ui
