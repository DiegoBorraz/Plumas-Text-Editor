#pragma once

#include "plumas/ui/AppState.hpp"

#include <string_view>

#include <gtk/gtk.h>

namespace plumas::ui {

GtkWidget* createEditorView(AppState* state);
// Limits undo and connects the editor signals for a new tab's buffer.
void attachEditorBuffer(AppState* state, GtkTextBuffer* buffer);
// Replaces the active tab's text without marking it dirty. `text` must be
// valid UTF-8 without NUL bytes.
void setEditorText(AppState* state, std::string_view text);
// Re-reads the colours the editor paints itself after a light/dark switch.
void applyEditorColors(AppState* state);
// Clamps to 10-500% and updates the status bar.
void setEditorZoom(AppState* state, int percent);
// The editor buffer is the only copy of the text; this hands out a snapshot.
GCharPtr copyEditorText(AppState* state);

} // namespace plumas::ui
