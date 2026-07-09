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

EXE="$BUILD_DIR/plumas-text-editor.exe"
if [[ ! -x "$EXE" ]]; then
    echo "Erro: binario nao encontrado em $EXE" >&2
    exit 1
fi

rm -rf "$STAGING"
mkdir -p "$STAGING/bin" "$STAGING/share/glib-2.0/schemas" "$STAGING/lib/gdk-pixbuf-2.0/2.10.0/loaders"

cp "$EXE" "$STAGING/plumas-text-editor.exe"
cp "$ROOT/packaging/debian/LICENSE" "$STAGING/LICENSE"

mapfile -t runtime_libs < <(ldd "$EXE" | awk '/=>/ {print $3}' | grep -E "${MINGW_PREFIX}/|/ucrt64/" | sort -u)
for lib in "${runtime_libs[@]}"; do
    if [[ -f "$lib" ]]; then
        cp -u "$lib" "$STAGING/bin/"
    fi
done

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
    if command -v gdk-pixbuf-query-loaders >/dev/null 2>&1; then
        gdk-pixbuf-query-loaders > "$STAGING/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache"
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
