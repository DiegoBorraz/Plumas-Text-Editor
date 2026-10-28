#include "plumas/ui/Application.hpp"

#include "plumas/ui/AppState.hpp"
#include "plumas/ui/Dialogs.hpp"
#include "plumas/ui/EditorView.hpp"
#include "plumas/ui/MainWindow.hpp"
#include "plumas/ui/Resources.hpp"

#include <adwaita.h>
#include <gtk/gtk.h>

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

namespace plumas::ui {

namespace {

void loadStyles() {
    GtkCssProvider* provider = gtk_css_provider_new();

    if (hasBundledResource(kStylesFileName)) {
        gtk_css_provider_load_from_resource(provider, resourcePath(kStylesFileName).c_str());
    } else if (const std::optional<std::string> stylesPath = findFilesystemResourcePath(kStylesFileName);
               stylesPath.has_value()) {
        gtk_css_provider_load_from_path(provider, stylesPath->c_str());
    } else {
        std::cerr << "pluma-text: styles.css not found\n";
        g_object_unref(provider);
        return;
    }

    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(provider);
}

// Follows the system light/dark preference: styles-dark.css only redefines
// the palette's colour names, so it is layered on top of styles.css.
void applyColorScheme(AppState* state) {
    const bool dark = adw_style_manager_get_dark(adw_style_manager_get_default());
    GdkDisplay* display = gdk_display_get_default();

    if (dark && state->darkProvider == nullptr && hasBundledResource(kDarkStylesFileName)) {
        state->darkProvider = gtk_css_provider_new();
        gtk_css_provider_load_from_resource(
            state->darkProvider, resourcePath(kDarkStylesFileName).c_str());
        gtk_style_context_add_provider_for_display(
            display,
            GTK_STYLE_PROVIDER(state->darkProvider),
            GTK_STYLE_PROVIDER_PRIORITY_USER + 1);
    } else if (!dark && state->darkProvider != nullptr) {
        gtk_style_context_remove_provider_for_display(
            display, GTK_STYLE_PROVIDER(state->darkProvider));
        g_clear_object(&state->darkProvider);
    }

    state->darkTheme = dark;
    if (state->textView != nullptr) {
        applyEditorColors(state);
    }
}

void onDarkChanged(GObject* /*styleManager*/, GParamSpec* /*property*/, gpointer userData) {
    applyColorScheme(static_cast<AppState*>(userData));
}

// libadwaita 1.6+ (GTK 4.16+, e.g. MSYS2 on Windows) styles its widgets with
// CSS variables instead of the named accent colours in styles.css. GTK 4.14
// cannot parse variables, so this file is only loaded where they exist.
void loadAdwaitaVariables() {
    if (gtk_get_minor_version() < 16 || !hasBundledResource(kAdwaitaVariablesFileName)) {
        return;
    }
    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_resource(provider, resourcePath(kAdwaitaVariablesFileName).c_str());
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(provider);
}

void onAppStartup(AdwApplication* /*app*/, gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);
    loadStyles();
    loadAdwaitaVariables();
    applyColorScheme(state);
    g_signal_connect(
        adw_style_manager_get_default(), "notify::dark", G_CALLBACK(onDarkChanged), state);
}

void ensureMainWindow(AppState* state, AdwApplication* app) {
    if (state->window != nullptr) {
        return;
    }

    state->app = app;
    buildMainWindow(state);
}

void onActivate(AdwApplication* app, gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);

    if (state->window != nullptr) {
        presentMainWindow(state);
        return;
    }

    ensureMainWindow(state, app);
    presentMainWindow(state);
}

void onOpen(
    AdwApplication* app,
    GFile** files,
    int fileCount,
    const char* /*hint*/,
    gpointer userData) {
    AppState* state = static_cast<AppState*>(userData);

    ensureMainWindow(state, app);

    if (fileCount > 0) {
        char* path = g_file_get_path(files[0]);
        if (path != nullptr) {
            tryOpenPath(state, path);
            g_free(path);
        }
    }

    presentMainWindow(state);
}

} // namespace

int run(int argc, char** argv) {
    // The software renderer uses ~100 MB less RSS than the GL/Vulkan renderers
    // and is fast enough for a plain text editor. GSK_RENDERER still overrides it.
    g_setenv("GSK_RENDERER", "cairo", FALSE);
    // X11 WM_CLASS comes from the program name; matching the app ID lets the
    // desktop file (StartupWMClass) pair the window with its icon.
    g_set_prgname(kAppId);
    adw_init();

    AppState state{};
    state.config.load();

    const GApplicationFlags appFlags = static_cast<GApplicationFlags>(
        G_APPLICATION_DEFAULT_FLAGS | G_APPLICATION_HANDLES_OPEN);

    state.app = ADW_APPLICATION(adw_application_new(kAppId, appFlags));
    g_signal_connect(state.app, "startup", G_CALLBACK(onAppStartup), &state);
    g_signal_connect(state.app, "activate", G_CALLBACK(onActivate), &state);
    g_signal_connect(state.app, "open", G_CALLBACK(onOpen), &state);

    const int status = g_application_run(G_APPLICATION(state.app), argc, argv);
    g_object_unref(state.app);
    return status;
}

} // namespace plumas::ui
