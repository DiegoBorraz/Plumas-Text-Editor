#include "plumas/ui/Tabs.hpp"

#include "plumas/ui/Dialogs.hpp"
#include "plumas/ui/EditorView.hpp"
#include "plumas/ui/SearchReplace.hpp"
#include "plumas/ui/UiHelpers.hpp"

#include "plumas/core/FileIO.hpp"
#include "plumas/platform/Paths.hpp"

#include <algorithm>

namespace plumas::ui {

namespace {

constexpr int kTabLabelMaxChars = 24;
// Marks the first visible line of a tab that is not shown, to restore its view.
constexpr const char kViewTopMark[] = "plumas-view-top";

EditorTab* tabOf(GtkWidget* widget) {
    return static_cast<EditorTab*>(g_object_get_data(G_OBJECT(widget), "plumas-tab"));
}

std::string tabName(const EditorTab& tab) {
    const std::string& path = tab.document->filePath();
    if (path.empty()) {
        return kUntitledName;
    }
    return core::pathToUtf8(core::pathFromUtf8(path).filename());
}

void rememberViewTop(AppState* state) {
    GtkTextView* view = GTK_TEXT_VIEW(state->textView);
    GdkRectangle visible;
    gtk_text_view_get_visible_rect(view, &visible);
    GtkTextIter top;
    gtk_text_view_get_iter_at_location(view, &top, visible.x, visible.y);

    GtkTextBuffer* buffer = state->activeTab->buffer;
    if (GtkTextMark* mark = gtk_text_buffer_get_mark(buffer, kViewTopMark)) {
        gtk_text_buffer_move_mark(buffer, mark, &top);
    } else {
        gtk_text_buffer_create_mark(buffer, kViewTopMark, &top, TRUE);
    }
}

// After gtk_text_view_set_buffer() the view measures lines in background idles
// and positions are estimates until it finishes; a low-priority idle runs
// only once those are done, so the scroll lands where the user left it.
gboolean restoreViewTop(gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);
    if (state->activeTab != nullptr) {
        if (GtkTextMark* mark = gtk_text_buffer_get_mark(state->activeTab->buffer, kViewTopMark)) {
            gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(state->textView), mark, 0.0, TRUE, 0.0, 0.0);
        }
    }
    return G_SOURCE_REMOVE;
}

// Drops a tab without asking. The caller decides what happens to the window.
void removeTab(AppState* state, EditorTab* tab) {
    const auto position = std::find_if(
        state->tabs.begin(),
        state->tabs.end(),
        [tab](const std::unique_ptr<EditorTab>& entry) { return entry.get() == tab; });
    if (position == state->tabs.end()) {
        return;
    }

    if (tab == state->activeTab) {
        abandonReplaceAll(state);
        state->activeTab = nullptr;
    }
    gtk_box_remove(GTK_BOX(state->tabBox), tab->tabWidget);

    // Like a browser, focus moves to the tab that slides into this place.
    const std::size_t index = static_cast<std::size_t>(position - state->tabs.begin());
    state->tabs.erase(position);
    if (state->activeTab == nullptr && !state->tabs.empty()) {
        activateTab(state, state->tabs[std::min(index, state->tabs.size() - 1)].get());
    }
}

void closeActiveTabNow(AppState* state) {
    removeTab(state, state->activeTab);
    if (state->tabs.empty()) {
        gtk_window_destroy(state->window);
    }
}

void closeActiveTabAndContinueQuit(AppState* state) {
    removeTab(state, state->activeTab);
    requestQuit(state);
}

bool containsTab(const AppState* state, const EditorTab* tab) {
    return std::any_of(
        state->tabs.begin(),
        state->tabs.end(),
        [tab](const std::unique_ptr<EditorTab>& entry) { return entry.get() == tab; });
}

struct PendingTabClose {
    AppState* state;
    EditorTab* tab;
};

// Closing destroys the widget whose click handler is running, and GTK keeps
// using that widget after the handler returns; so close from an idle callback.
void closeTabLater(AppState* state, EditorTab* tab) {
    g_idle_add_full(
        G_PRIORITY_DEFAULT_IDLE,
        +[](gpointer userData) -> gboolean {
            const std::unique_ptr<PendingTabClose> pending(static_cast<PendingTabClose*>(userData));
            if (containsTab(pending->state, pending->tab)) {
                closeTab(pending->state, pending->tab);
            }
            return G_SOURCE_REMOVE;
        },
        new PendingTabClose{state, tab},
        nullptr);
}

void onTabPressed(GtkGestureClick* gesture, gint /*nPress*/, gdouble /*x*/, gdouble /*y*/, gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);
    EditorTab* tab = tabOf(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture)));
    if (gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture)) == GDK_BUTTON_MIDDLE) {
        closeTabLater(state, tab);
        return;
    }
    activateTab(state, tab);
}

void onTabCloseClicked(GtkButton* button, gpointer userData) {
    closeTabLater(static_cast<AppState*>(userData), tabOf(GTK_WIDGET(button)));
}

void onNewTabClicked(GtkButton* /*button*/, gpointer userData) {
    newTab(static_cast<AppState*>(userData));
}

void createTabWidget(AppState* state, EditorTab* tab) {
    tab->tabWidget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(tab->tabWidget, "editor-tab");
    g_object_set_data(G_OBJECT(tab->tabWidget), "plumas-tab", tab);

    tab->tabLabel = gtk_label_new(nullptr);
    gtk_label_set_ellipsize(GTK_LABEL(tab->tabLabel), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(tab->tabLabel), kTabLabelMaxChars);
    gtk_label_set_xalign(GTK_LABEL(tab->tabLabel), 0.0f);
    gtk_widget_set_hexpand(tab->tabLabel, TRUE);

    GtkWidget* closeButton = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_widget_add_css_class(closeButton, "flat");
    gtk_widget_add_css_class(closeButton, "tab-close");
    gtk_widget_set_tooltip_text(closeButton, "Close tab (Ctrl+W)");
    gtk_widget_set_valign(closeButton, GTK_ALIGN_CENTER);
    g_object_set_data(G_OBJECT(closeButton), "plumas-tab", tab);
    g_signal_connect(closeButton, "clicked", G_CALLBACK(onTabCloseClicked), state);

    gtk_box_append(GTK_BOX(tab->tabWidget), tab->tabLabel);
    gtk_box_append(GTK_BOX(tab->tabWidget), closeButton);

    // Any button: primary selects, middle closes, as in browsers.
    GtkGesture* click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), 0);
    g_signal_connect(click, "pressed", G_CALLBACK(onTabPressed), state);
    gtk_widget_add_controller(tab->tabWidget, GTK_EVENT_CONTROLLER(click));

    gtk_box_append(GTK_BOX(state->tabBox), tab->tabWidget);
    updateTabLabel(tab);
}

} // namespace

GtkWidget* createTabStrip(AppState* state) {
    GtkWidget* strip = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_add_css_class(strip, "tab-strip");
    // Set explicitly so the labels' hexpand does not stretch tabs across the row.
    gtk_widget_set_hexpand(strip, FALSE);

    state->tabBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);

    // Scrolls sideways once the tabs no longer fit, instead of growing the window.
    GtkWidget* scroller = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(scroller),
        GTK_POLICY_EXTERNAL,
        GTK_POLICY_NEVER);
    gtk_scrolled_window_set_propagate_natural_width(GTK_SCROLLED_WINDOW(scroller), TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), state->tabBox);

    GtkWidget* newButton = gtk_button_new_from_icon_name("list-add-symbolic");
    gtk_widget_add_css_class(newButton, "flat");
    gtk_widget_add_css_class(newButton, "new-tab");
    gtk_widget_set_tooltip_text(newButton, "New tab (Ctrl+T)");
    gtk_widget_set_valign(newButton, GTK_ALIGN_CENTER);
    g_signal_connect(newButton, "clicked", G_CALLBACK(onNewTabClicked), state);

    gtk_box_append(GTK_BOX(strip), scroller);
    gtk_box_append(GTK_BOX(strip), newButton);
    return strip;
}

EditorTab* addTab(
    AppState* state,
    std::unique_ptr<core::Document> document,
    const std::string_view text,
    const bool usesCrlf) {
    auto tab = std::make_unique<EditorTab>();
    tab->document = std::move(document);
    tab->usesCrlf = usesCrlf;
    tab->buffer = gtk_text_buffer_new(nullptr);
    attachEditorBuffer(state, tab->buffer);
    prepareSearchBuffer(tab->buffer);

    EditorTab* added = tab.get();
    state->tabs.push_back(std::move(tab));
    createTabWidget(state, added);
    activateTab(state, added);
    setEditorText(state, text);
    return added;
}

void newTab(AppState* state) {
    addTab(state, std::make_unique<core::Document>(), "", platform::usesCrlfByDefault());
    gtk_widget_grab_focus(state->textView);
}

void activateTab(AppState* state, EditorTab* tab) {
    if (tab == nullptr || tab == state->activeTab) {
        return;
    }

    if (state->activeTab != nullptr) {
        rememberViewTop(state);
        // A running Replace All would otherwise land in the newly shown buffer.
        abandonReplaceAll(state);
        gtk_widget_remove_css_class(state->activeTab->tabWidget, "active");
    }

    state->activeTab = tab;
    gtk_widget_add_css_class(tab->tabWidget, "active");
    // Each tab's view is restored from its own mark below. Starting from the top
    // also keeps GTK 4.14 from drawing the old, now out-of-range scrollbar
    // ("Trying to snapshot GtkGizmo without a current allocation").
    gtk_adjustment_set_value(
        gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(state->editorScroll)), 0.0);
    gtk_text_view_set_buffer(GTK_TEXT_VIEW(state->textView), tab->buffer);
    if (gtk_text_buffer_get_mark(tab->buffer, kViewTopMark) != nullptr) {
        g_idle_add_full(G_PRIORITY_LOW, restoreViewTop, state, nullptr);
    }
    updateStatus(state);
    gtk_widget_queue_draw(state->lineGutter);
}

void cycleTab(AppState* state, const int step) {
    const auto count = static_cast<long>(state->tabs.size());
    if (count < 2) {
        return;
    }

    const auto position = std::find_if(
        state->tabs.begin(),
        state->tabs.end(),
        [state](const std::unique_ptr<EditorTab>& entry) { return entry.get() == state->activeTab; });
    const long index = static_cast<long>(position - state->tabs.begin());
    const long next = ((index + step) % count + count) % count;
    activateTab(state, state->tabs[static_cast<std::size_t>(next)].get());
}

EditorTab* findTabByPath(AppState* state, const std::string& path) {
    for (const std::unique_ptr<EditorTab>& tab : state->tabs) {
        if (!tab->document->filePath().empty() && tab->document->filePath() == path) {
            return tab.get();
        }
    }
    return nullptr;
}

bool isReusableTab(const EditorTab& tab) {
    return tab.document->filePath().empty() && !tab.document->isDirty()
        && gtk_text_buffer_get_char_count(tab.buffer) == 0;
}

void updateTabLabel(EditorTab* tab) {
    const std::string name = tabName(*tab);
    // Win11 Notepad marks unsaved changes with a dot next to the name.
    const std::string text = tab->document->isDirty() ? name + "  \xE2\x80\xA2" : name;
    gtk_label_set_text(GTK_LABEL(tab->tabLabel), text.c_str());

    const std::string& path = tab->document->filePath();
    gtk_widget_set_tooltip_text(tab->tabWidget, path.empty() ? nullptr : path.c_str());
}

void closeTab(AppState* state, EditorTab* tab) {
    if (tab == nullptr) {
        return;
    }

    activateTab(state, tab);
    if (tab->document->isDirty()) {
        showUnsavedDialog(state, closeActiveTabNow);
        return;
    }
    closeActiveTabNow(state);
}

void requestQuit(AppState* state) {
    for (const std::unique_ptr<EditorTab>& tab : state->tabs) {
        if (tab->document->isDirty()) {
            activateTab(state, tab.get());
            showUnsavedDialog(state, closeActiveTabAndContinueQuit);
            return;
        }
    }
    gtk_window_destroy(state->window);
}

bool hasUnsavedTabs(const AppState* state) {
    return std::any_of(
        state->tabs.begin(),
        state->tabs.end(),
        [](const std::unique_ptr<EditorTab>& tab) { return tab->document->isDirty(); });
}

} // namespace plumas::ui
