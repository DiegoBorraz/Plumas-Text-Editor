#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PACKAGING_DIR="$ROOT/packaging/debian"
BUILD_DIR="$ROOT/build"
STAGING="$PACKAGING_DIR/staging"
OUTPUT_DIR="$PACKAGING_DIR/dist"
ICON_SOURCE="$ROOT/plumas.png"
ICON_NAME="plumas-editor-texto"
ICON_SIZES=(16 24 32 48 64 128 256 512)

PKG_NAME="plumas-editor-texto"
VERSION="$(grep '^project(plumas-editor-texto' "$ROOT/CMakeLists.txt" | sed -E 's/.*VERSION ([0-9.]+).*/\1/')"
ARCH="$(dpkg --print-architecture)"
DEB_FILE="${PKG_NAME}_${VERSION}_${ARCH}.deb"

# shellcheck source=metadata.sh
source "$PACKAGING_DIR/metadata.sh"

usage() {
    cat <<EOF
Uso: $(basename "$0") [opcoes]

  Compila o projeto em Release e gera um pacote .deb para Debian/Ubuntu.

Opcoes:
  --skip-build    Nao recompila; usa o binario existente em build/
  --version VER   Sobrescreve a versao lida do CMakeLists.txt
  -h, --help      Mostra esta ajuda
EOF
}

SKIP_BUILD=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --skip-build)
            SKIP_BUILD=1
            shift
            ;;
        --version)
            VERSION="$2"
            DEB_FILE="${PKG_NAME}_${VERSION}_${ARCH}.deb"
            shift 2
            ;;
        -h | --help)
            usage
            exit 0
            ;;
        *)
            echo "Opcao desconhecida: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Erro: comando obrigatorio nao encontrado: $1" >&2
        exit 1
    fi
}

resize_icon() {
    local size="$1"
    local output="$2"
    if command -v magick >/dev/null 2>&1; then
        magick "$ICON_SOURCE" -resize "${size}x${size}" "$output"
        return
    fi
    if command -v convert >/dev/null 2>&1; then
        convert "$ICON_SOURCE" -resize "${size}x${size}" "$output"
        return
    fi
    if python3 - <<'PY' "$ICON_SOURCE" "$size" "$output"
from PIL import Image
import sys

source, size_text, output = sys.argv[1:4]
size = int(size_text)
with Image.open(source) as image:
    resized = image.convert("RGBA").resize((size, size), Image.Resampling.LANCZOS)
    resized.save(output, format="PNG")
PY
    then
        return
    fi
    echo "Erro: instale imagemagick ou pillow (sudo apt install imagemagick python3-pil)" >&2
    exit 1
}

install_hicolor_icons() {
    echo "==> Gerando icones do tema hicolor a partir de plumas.png..."
    for size in "${ICON_SIZES[@]}"; do
        local icon_dir="$STAGING/usr/share/icons/hicolor/${size}x${size}/apps"
        mkdir -p "$icon_dir"
        resize_icon "$size" "$icon_dir/${ICON_NAME}.png"
        chmod 644 "$icon_dir/${ICON_NAME}.png"
    done
}

write_postinst() {
    cat >"$STAGING/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e

if [ "$1" = configure ]; then
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor || true
    fi
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database -q /usr/share/applications || true
    fi
fi
EOF
    chmod 755 "$STAGING/DEBIAN/postinst"
}

write_postrm() {
    cat >"$STAGING/DEBIAN/postrm" <<'EOF'
#!/bin/sh
set -e

if [ "$1" = remove ] || [ "$1" = abort-install ] || [ "$1" = purge ]; then
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor || true
    fi
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database -q /usr/share/applications || true
    fi
fi
EOF
    chmod 755 "$STAGING/DEBIAN/postrm"
}

write_copyright() {
    local current_year
    current_year="$(date +%Y)"
    local license_file="$PACKAGING_DIR/LICENSE"
    cat >"$STAGING/usr/share/doc/${PKG_NAME}/copyright" <<EOF
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: ${PKG_NAME}
Upstream-Contact: ${PKG_MAINTAINER}
Source: ${PKG_HOMEPAGE}

Files: *
Copyright: ${current_year} ${PKG_DEVELOPER}
License: ${PKG_LICENSE}
EOF
    sed 's/^/ /' "$license_file" >>"$STAGING/usr/share/doc/${PKG_NAME}/copyright"
    chmod 644 "$STAGING/usr/share/doc/${PKG_NAME}/copyright"
    install -m 644 "$license_file" "$STAGING/usr/share/doc/${PKG_NAME}/LICENSE"
}

write_control() {
    cat >"$STAGING/DEBIAN/control" <<EOF
Package: ${PKG_NAME}
Version: ${VERSION}
Section: editors
Priority: optional
Architecture: ${ARCH}
Depends: libgtk-4-1 (>= 4.14), libadwaita-1-0 (>= 1.5)
Maintainer: ${PKG_MAINTAINER}
Homepage: ${PKG_HOMEPAGE}
Description: ${PKG_SUMMARY}
$(echo "$PKG_DESCRIPTION" | sed 's/^/ /')
EOF
}

require_command cmake
require_command dpkg-deb
require_command dpkg

if [[ ! -f "$ICON_SOURCE" ]]; then
    echo "Erro: icone de alta resolucao nao encontrado em $ICON_SOURCE" >&2
    exit 1
fi

if [[ "$SKIP_BUILD" -eq 0 ]]; then
    echo "==> Configurando build Release..."
    cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
    echo "==> Compilando..."
    cmake --build "$BUILD_DIR" -j"$(nproc)"
fi

BINARY="$BUILD_DIR/plumas-editor-texto"
if [[ ! -x "$BINARY" ]]; then
    echo "Erro: binario nao encontrado em $BINARY" >&2
    exit 1
fi

echo "==> Montando pacote Debian ${VERSION} (${ARCH})..."
rm -rf "$STAGING"
mkdir -p \
    "$STAGING/DEBIAN" \
    "$STAGING/usr/lib/plumas-editor-texto/resources" \
    "$STAGING/usr/bin" \
    "$STAGING/usr/share/applications" \
    "$STAGING/usr/share/doc/${PKG_NAME}"

install -m 755 "$BINARY" "$STAGING/usr/lib/plumas-editor-texto/plumas-editor-texto"
install -m 644 "$ROOT/src/ui/styles.css" "$STAGING/usr/lib/plumas-editor-texto/resources/styles.css"
install -m 644 "$ROOT/resources/plumas-icon.webp" "$STAGING/usr/lib/plumas-editor-texto/resources/plumas-icon.webp"
ln -sf ../lib/plumas-editor-texto/plumas-editor-texto "$STAGING/usr/bin/plumas-editor-texto"
sed "s/@DEVELOPER@/${PKG_DEVELOPER}/" "$PACKAGING_DIR/plumas-editor-texto.desktop" \
    >"$STAGING/usr/share/applications/plumas-editor-texto.desktop"
chmod 644 "$STAGING/usr/share/applications/plumas-editor-texto.desktop"

install_hicolor_icons
write_postinst
write_postrm
write_copyright
write_control

mkdir -p "$OUTPUT_DIR"
rm -f "$OUTPUT_DIR/$DEB_FILE"
dpkg-deb --build --root-owner-group "$STAGING" "$OUTPUT_DIR/$DEB_FILE"

echo ""
echo "Pacote criado: $OUTPUT_DIR/$DEB_FILE"
echo ""
echo "Instalar:"
echo "  sudo apt install ./packaging/debian/dist/$DEB_FILE"
echo ""
echo "Ou:"
echo "  sudo dpkg -i $OUTPUT_DIR/$DEB_FILE"
echo "  sudo apt-get install -f"
