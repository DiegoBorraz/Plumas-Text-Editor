#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-win}"
STAGING="$ROOT/packaging/windows/staging"
MINGW_PREFIX="${MINGW_PREFIX:-/ucrt64}"

if [[ -z "${MSYSTEM:-}" ]]; then
    echo "Execute este script no shell MSYS2 UCRT64." >&2
    exit 1
fi

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Erro: comando obrigatorio nao encontrado: $1" >&2
        exit 1
    fi
}

require_command ldd
require_command cp
require_command mkdir

EXE="$BUILD_DIR/pluma-text.exe"
if [[ ! -x "$EXE" ]]; then
    echo "Erro: binario nao encontrado em $EXE" >&2
    exit 1
fi

rm -rf "$STAGING"
mkdir -p "$STAGING/bin" "$STAGING/share/glib-2.0/schemas" "$STAGING/lib/gdk-pixbuf-2.0/2.10.0/loaders"

# The exe lives in bin/ next to the DLLs: Windows only looks for DLLs in the
# exe's own directory (and system paths), and GTK finds share/ and lib/ as
# siblings of that directory.
cp "$EXE" "$STAGING/bin/pluma-text.exe"
cp "$ROOT/packaging/debian/LICENSE" "$STAGING/LICENSE"

# Copies every MinGW DLL a binary loads, directly or indirectly.
copy_runtime_dlls() {
    local binary="$1"
    local lib
    while IFS= read -r lib; do
        if [[ -f "$lib" ]]; then
            cp -u "$lib" "$STAGING/bin/"
        fi
    done < <(ldd "$binary" | awk '/=>/ {print $3}' | grep -E "${MINGW_PREFIX}/|/ucrt64/" | sort -u)
}

copy_runtime_dlls "$EXE"

GLIB_SCHEMAS_SRC="${MINGW_PREFIX}/share/glib-2.0/schemas"
if [[ -d "$GLIB_SCHEMAS_SRC" ]]; then
    cp -r "${GLIB_SCHEMAS_SRC}/." "$STAGING/share/glib-2.0/schemas/"
    if command -v glib-compile-schemas >/dev/null 2>&1; then
        glib-compile-schemas "$STAGING/share/glib-2.0/schemas"
    fi
fi

GDK_PIXBUF_LOADERS="${MINGW_PREFIX}/lib/gdk-pixbuf-2.0/2.10.0/loaders"
if [[ -d "$GDK_PIXBUF_LOADERS" ]]; then
    cp -r "${GDK_PIXBUF_LOADERS}/." "$STAGING/lib/gdk-pixbuf-2.0/2.10.0/loaders/"
    # Loaders are plugins: their own DLLs (librsvg for the SVG icons, ...)
    # never show up in the exe's ldd output.
    for loader in "$GDK_PIXBUF_LOADERS"/*.dll; do
        copy_runtime_dlls "$loader"
    done

    if command -v gdk-pixbuf-query-loaders >/dev/null 2>&1; then
        # The cache lists loaders by absolute path on this machine; strip the
        # prefix so gdk-pixbuf resolves them relative to the install.
        prefix_windows="$(cygpath -m "$MINGW_PREFIX")"
        gdk-pixbuf-query-loaders \
            | sed -e "s|${prefix_windows}/||g" -e "s|${MINGW_PREFIX}/||g" \
            > "$STAGING/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache"
        if grep -q -e "$prefix_windows" "$STAGING/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache"; then
            echo "Erro: loaders.cache ainda tem caminhos absolutos" >&2
            exit 1
        fi
    fi
fi

for share_dir in icons themes gtk-4.0; do
    src="${MINGW_PREFIX}/share/${share_dir}"
    if [[ -d "$src" ]]; then
        mkdir -p "$STAGING/share/${share_dir}"
        cp -r "${src}/." "$STAGING/share/${share_dir}/"
    fi
done

echo "Bundle criado em $STAGING"
