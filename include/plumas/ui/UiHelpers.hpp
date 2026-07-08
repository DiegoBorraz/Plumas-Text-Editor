#pragma once

#include "plumas/ui/AppState.hpp"

namespace plumas::core {
class Document;
}

namespace plumas::ui {

bool isSaveAvailable(const core::Document& document);
void showToast(AppState* state, const char* message);
void updateStatus(AppState* state);
void syncWindowTheme(AppState* state);
void setLabelStyle(GtkLabel* label, int fontSizePt, bool bold);

} // namespace plumas::ui
