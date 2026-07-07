#include "plumas/ui/Application.hpp"

#include "plumas/ui/AppState.hpp"
#include "plumas/ui/Dialogs.hpp"
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
        std::cerr << "plumas: styles.css not found\n";
        g_object_unref(provider);
        return;
    }

    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(provider);
}

void onAppStartup(AdwApplication* /*app*/, gpointer /*userData*/) {
    loadStyles();
}

void ensureMainWindow(AppState* state, AdwApplication* app) {
    if (state->window != nullptr) {
        return;
    }

    state->app = app;
    buildMainWindow(state);
}

void openFileArgument(AppState* state, const std::string& path) {
    if (state->document != nullptr && state->document->isDirty()) {
        state->pendingOpenPath = path;
        runWithUnsavedCheck(state, openPendingPath);
        return;
    }

    tryOpenPath(state, path);
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
            openFileArgument(state, path);
            g_free(path);
        }
    }

    presentMainWindow(state);
}

} // namespace

int run(int argc, char** argv) {
    adw_init();

    AppState state{};
    state.config.load();

    const GApplicationFlags appFlags = static_cast<GApplicationFlags>(
        G_APPLICATION_DEFAULT_FLAGS | G_APPLICATION_HANDLES_OPEN);

    state.app = ADW_APPLICATION(adw_application_new(kAppId, appFlags));
    g_signal_connect(state.app, "startup", G_CALLBACK(onAppStartup), nullptr);
    g_signal_connect(state.app, "activate", G_CALLBACK(onActivate), &state);
    g_signal_connect(state.app, "open", G_CALLBACK(onOpen), &state);

    const int status = g_application_run(G_APPLICATION(state.app), argc, argv);
    g_object_unref(state.app);
    return status;
}

} // namespace plumas::ui
