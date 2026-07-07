#pragma once

#include "plumas/ui/AppState.hpp"

#include <gtk/gtk.h>

namespace plumas::ui {

GtkWidget* createTitleBar(AppState* state);

} // namespace plumas::ui
