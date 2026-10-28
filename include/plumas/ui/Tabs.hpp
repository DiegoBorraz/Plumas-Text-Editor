#pragma once

#include "plumas/ui/AppState.hpp"

#include <memory>
#include <string>
#include <string_view>

namespace plumas::ui {

// The strip of tab buttons plus the "+" button.
GtkWidget* createTabStrip(AppState* state);

// Adds a tab holding `text` and makes it active.
EditorTab* addTab(
    AppState* state,
    std::unique_ptr<core::Document> document,
    std::string_view text,
    bool usesCrlf);
void newTab(AppState* state);
void activateTab(AppState* state, EditorTab* tab);
void cycleTab(AppState* state, int step);
EditorTab* findTabByPath(AppState* state, const std::string& path);
// An untitled, unchanged, empty tab that opening a file may take over.
bool isReusableTab(const EditorTab& tab);
void updateTabLabel(EditorTab* tab);

// Asks about unsaved changes first. Closing the last tab closes the window.
void closeTab(AppState* state, EditorTab* tab);
// Asks about every tab with unsaved changes, then closes the window.
void requestQuit(AppState* state);
bool hasUnsavedTabs(const AppState* state);
// False once the tab has been closed; for callbacks that outlive a tab.
bool hasTab(const AppState* state, const EditorTab* tab);

} // namespace plumas::ui
