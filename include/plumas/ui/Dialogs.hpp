#pragma once

#include "plumas/ui/AppState.hpp"

#include <memory>
#include <string>

namespace plumas::ui {

void showOpenDialog(AppState* state);
void showSaveDialog(AppState* state);
void showUnsavedDialog(AppState* state, std::unique_ptr<PendingAction> pending);
void showOverwriteDialog(AppState* state, std::unique_ptr<PendingSave> pending);

void runWithUnsavedCheck(AppState* state, void (*action)(AppState*));
void newDocument(AppState* state);
void saveDocument(AppState* state, bool saveAs);
void performSave(AppState* state, const std::string& path);
void tryOpenPath(AppState* state, const std::string& path);
void trySavePath(AppState* state, const std::string& path);
void openPendingPath(AppState* state);
void actionShowOpenDialog(AppState* state);
void closeWindow(AppState* state);

} // namespace plumas::ui
