#include "plumas/ui/MenuBar.hpp"

#include "plumas/ui/Dialogs.hpp"
#include "plumas/ui/EditorView.hpp"
#include "plumas/ui/SearchReplace.hpp"
#include "plumas/ui/UiHelpers.hpp"

namespace plumas::ui {

namespace {

constexpr int kZoomStepPercent = 10;

AppState* toState(gpointer userData) {
    return static_cast<AppState*>(userData);
}

void onNew(GSimpleAction* /*action*/, GVariant* /*parameter*/, gpointer userData) {
    runWithUnsavedCheck(toState(userData), newDocument);
}

void onOpen(GSimpleAction* /*action*/, GVariant* /*parameter*/, gpointer userData) {
    runWithUnsavedCheck(toState(userData), showOpenDialog);
}

void onSave(GSimpleAction* /*action*/, GVariant* /*parameter*/, gpointer userData) {
    AppState* state = toState(userData);
    if (isSaveAvailable(*state->document)) {
        saveDocument(state, false);
    }
}

void onSaveAs(GSimpleAction* /*action*/, GVariant* /*parameter*/, gpointer userData) {
    saveDocument(toState(userData), true);
}

void onQuit(GSimpleAction* /*action*/, GVariant* /*parameter*/, gpointer userData) {
    runWithUnsavedCheck(toState(userData), closeWindow);
}

// The text view already binds these keys; the menu only forwards to it.
void forwardToEditor(gpointer userData, const char* action) {
    gtk_widget_activate_action(toState(userData)->textView, action, nullptr);
}

void onUndo(GSimpleAction* /*action*/, GVariant* /*parameter*/, gpointer userData) {
    forwardToEditor(userData, "text.undo");
}

void onCut(GSimpleAction* /*action*/, GVariant* /*parameter*/, gpointer userData) {
    forwardToEditor(userData, "clipboard.cut");
}

void onCopy(GSimpleAction* /*action*/, GVariant* /*parameter*/, gpointer userData) {
    forwardToEditor(userData, "clipboard.copy");
}

void onPaste(GSimpleAction* /*action*/, GVariant* /*parameter*/, gpointer userData) {
    forwardToEditor(userData, "clipboard.paste");
}

void onSelectAll(GSimpleAction* /*action*/, GVariant* /*parameter*/, gpointer userData) {
    forwardToEditor(userData, "selection.select-all");
}

void onFind(GSimpleAction* /*action*/, GVariant* /*parameter*/, gpointer userData) {
    toggleSearchReplaceBar(toState(userData));
}

void onZoomIn(GSimpleAction* /*action*/, GVariant* /*parameter*/, gpointer userData) {
    AppState* state = toState(userData);
    setEditorZoom(state, state->zoomPercent + kZoomStepPercent);
}

void onZoomOut(GSimpleAction* /*action*/, GVariant* /*parameter*/, gpointer userData) {
    AppState* state = toState(userData);
    setEditorZoom(state, state->zoomPercent - kZoomStepPercent);
}

void onZoomReset(GSimpleAction* /*action*/, GVariant* /*parameter*/, gpointer userData) {
    setEditorZoom(toState(userData), kDefaultZoomPercent);
}

void onWordWrapChanged(GSimpleAction* action, GVariant* value, gpointer userData) {
    gtk_text_view_set_wrap_mode(
        GTK_TEXT_VIEW(toState(userData)->textView),
        g_variant_get_boolean(value) ? GTK_WRAP_WORD_CHAR : GTK_WRAP_NONE);
    g_simple_action_set_state(action, value);
}

void onStatusBarChanged(GSimpleAction* action, GVariant* value, gpointer userData) {
    gtk_widget_set_visible(toState(userData)->statusBar, g_variant_get_boolean(value));
    g_simple_action_set_state(action, value);
}

void onLineNumbersChanged(GSimpleAction* action, GVariant* value, gpointer userData) {
    gtk_widget_set_visible(toState(userData)->lineGutter, g_variant_get_boolean(value));
    g_simple_action_set_state(action, value);
}

struct Accel {
    const char* action;
    const char* keys[4];
};

// Undo, clipboard and select-all are left to the focused widget so they keep
// working inside the find and replace entries.
constexpr Accel kAccels[] = {
    {"win.new", {"<Control>n", nullptr}},
    {"win.open", {"<Control>o", nullptr}},
    {"win.save", {"<Control>s", nullptr}},
    {"win.save-as", {"<Control><Shift>s", nullptr}},
    {"win.quit", {"<Control>q", nullptr}},
    {"win.find", {"<Control>f", nullptr}},
    {"win.zoom-in", {"<Control>plus", "<Control>equal", "<Control>KP_Add", nullptr}},
    {"win.zoom-out", {"<Control>minus", "<Control>KP_Subtract", nullptr}},
    {"win.zoom-reset", {"<Control>0", "<Control>KP_0", nullptr}},
};

void appendItem(GMenu* menu, const char* label, const char* action, const char* accel = nullptr) {
    GMenuItem* item = g_menu_item_new(label, action);
    if (accel != nullptr) {
        // Display only; the binding itself lives in kAccels or the text view.
        g_menu_item_set_attribute(item, "accel", "s", accel);
    }
    g_menu_append_item(menu, item);
    g_object_unref(item);
}

void appendSection(GMenu* menu, GMenu* section) {
    g_menu_append_section(menu, nullptr, G_MENU_MODEL(section));
    g_object_unref(section);
}

void appendSubmenu(GMenu* bar, const char* label, GMenu* submenu) {
    g_menu_append_submenu(bar, label, G_MENU_MODEL(submenu));
    g_object_unref(submenu);
}

GMenu* createFileMenu() {
    GMenu* menu = g_menu_new();
    GMenu* documents = g_menu_new();
    appendItem(documents, "New", "win.new");
    appendItem(documents, "Open…", "win.open");
    appendItem(documents, "Save", "win.save");
    appendItem(documents, "Save As…", "win.save-as");
    appendSection(menu, documents);

    GMenu* quit = g_menu_new();
    appendItem(quit, "Quit", "win.quit");
    appendSection(menu, quit);
    return menu;
}

GMenu* createEditMenu() {
    GMenu* menu = g_menu_new();
    GMenu* history = g_menu_new();
    appendItem(history, "Undo", "win.undo", "<Control>z");
    appendSection(menu, history);

    GMenu* clipboard = g_menu_new();
    appendItem(clipboard, "Cut", "win.cut", "<Control>x");
    appendItem(clipboard, "Copy", "win.copy", "<Control>c");
    appendItem(clipboard, "Paste", "win.paste", "<Control>v");
    appendSection(menu, clipboard);

    GMenu* search = g_menu_new();
    appendItem(search, "Find and Replace", "win.find");
    appendItem(search, "Select All", "win.select-all", "<Control>a");
    appendSection(menu, search);
    return menu;
}

GMenu* createViewMenu() {
    GMenu* menu = g_menu_new();
    GMenu* zoom = g_menu_new();
    appendItem(zoom, "Zoom In", "win.zoom-in");
    appendItem(zoom, "Zoom Out", "win.zoom-out");
    appendItem(zoom, "Restore Default Zoom", "win.zoom-reset");
    appendSection(menu, zoom);

    GMenu* toggles = g_menu_new();
    appendItem(toggles, "Status Bar", "win.status-bar");
    appendItem(toggles, "Word Wrap", "win.word-wrap");
    appendItem(toggles, "Line Numbers", "win.line-numbers");
    appendSection(menu, toggles);
    return menu;
}

} // namespace

void installWindowActions(AppState* state) {
    static const GActionEntry kEntries[] = {
        {"new", onNew, nullptr, nullptr, nullptr, {}},
        {"open", onOpen, nullptr, nullptr, nullptr, {}},
        {"save", onSave, nullptr, nullptr, nullptr, {}},
        {"save-as", onSaveAs, nullptr, nullptr, nullptr, {}},
        {"quit", onQuit, nullptr, nullptr, nullptr, {}},
        {"undo", onUndo, nullptr, nullptr, nullptr, {}},
        {"cut", onCut, nullptr, nullptr, nullptr, {}},
        {"copy", onCopy, nullptr, nullptr, nullptr, {}},
        {"paste", onPaste, nullptr, nullptr, nullptr, {}},
        {"select-all", onSelectAll, nullptr, nullptr, nullptr, {}},
        {"find", onFind, nullptr, nullptr, nullptr, {}},
        {"zoom-in", onZoomIn, nullptr, nullptr, nullptr, {}},
        {"zoom-out", onZoomOut, nullptr, nullptr, nullptr, {}},
        {"zoom-reset", onZoomReset, nullptr, nullptr, nullptr, {}},
        // A null activate on a boolean action toggles it through change_state.
        {"status-bar", nullptr, nullptr, "true", onStatusBarChanged, {}},
        {"word-wrap", nullptr, nullptr, "false", onWordWrapChanged, {}},
        {"line-numbers", nullptr, nullptr, "true", onLineNumbersChanged, {}},
    };
    g_action_map_add_action_entries(
        G_ACTION_MAP(state->window), kEntries, G_N_ELEMENTS(kEntries), state);

    for (const Accel& accel : kAccels) {
        gtk_application_set_accels_for_action(GTK_APPLICATION(state->app), accel.action, accel.keys);
    }
}

GtkWidget* createMenuBar() {
    GMenu* bar = g_menu_new();
    appendSubmenu(bar, "File", createFileMenu());
    appendSubmenu(bar, "Edit", createEditMenu());
    appendSubmenu(bar, "View", createViewMenu());

    GtkWidget* menuBar = gtk_popover_menu_bar_new_from_model(G_MENU_MODEL(bar));
    g_object_unref(bar);
    gtk_widget_add_css_class(menuBar, "plumas-menubar");
    return menuBar;
}

} // namespace plumas::ui
