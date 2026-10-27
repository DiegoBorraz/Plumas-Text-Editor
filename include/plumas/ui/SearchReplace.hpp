#pragma once

#include "plumas/ui/AppState.hpp"

namespace plumas::ui {

GtkWidget* createSearchReplaceBar(AppState* state);
void toggleSearchReplaceBar(AppState* state);
// Creates the per-buffer search state; call once for every tab's buffer.
void prepareSearchBuffer(GtkTextBuffer* buffer);
// Drops a running Replace All so its result is never applied to another document.
void abandonReplaceAll(AppState* state);

} // namespace plumas::ui
