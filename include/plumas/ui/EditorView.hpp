#pragma once

#include "plumas/ui/AppState.hpp"

#include <gtk/gtk.h>

namespace plumas::ui {

GtkWidget* createEditorView(AppState* state);
// Replaces the whole text without marking the document dirty.
void setEditorText(AppState* state, const char* text);
// The editor buffer is the only copy of the text; this hands out a snapshot.
GCharPtr copyEditorText(AppState* state);

} // namespace plumas::ui
