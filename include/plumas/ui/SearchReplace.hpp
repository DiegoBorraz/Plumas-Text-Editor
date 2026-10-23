#pragma once

#include "plumas/ui/AppState.hpp"

namespace plumas::ui {

GtkWidget* createSearchReplaceBar(AppState* state);
void toggleSearchReplaceBar(AppState* state);

} // namespace plumas::ui
