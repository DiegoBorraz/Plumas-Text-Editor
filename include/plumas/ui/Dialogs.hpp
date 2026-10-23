#pragma once

#include "plumas/ui/AppState.hpp"

#include <string>

namespace plumas::ui {

void showOpenDialog(AppState* state);
void showSaveDialog(AppState* state);
void showUnsavedDialog(AppState* state, AppAction action);

void runWithUnsavedCheck(AppState* state, AppAction action);
void newDocument(AppState* state);
void saveDocument(AppState* state, bool saveAs);
void tryOpenPath(AppState* state, const std::string& path);
void openPendingPath(AppState* state);
void closeWindow(AppState* state);

} // namespace plumas::ui
