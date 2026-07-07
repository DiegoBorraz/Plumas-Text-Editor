#pragma once

#include "plumas/ui/AppState.hpp"

#include <gtk/gtk.h>

namespace plumas::ui {

GtkWidget* createEditorView(AppState* state);
void refreshEditor(AppState* state);
void syncEditorToDocument(AppState* state);
bool isEditorWithinSizeLimit(AppState* state);

} // namespace plumas::ui
