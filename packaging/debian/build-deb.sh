#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PACKAGING_DIR="$ROOT/packaging/debian"
BUILD_DIR="$ROOT/build"
STAGING="$PACKAGING_DIR/staging"
OUTPUT_DIR="$PACKAGING_DIR/dist"
ICON_SOURCE="$ROOT/plumas.png"

PKG_NAME="plumas-editor-texto"
VERSION="$(grep '^project(plumas-editor-texto' "$ROOT/CMakeLists.txt" | sed -E 's/.*VERSION ([0-9.]+).*/\1/')"
ARCH="$(dpkg --print-architecture)"
DEB_FILE="${PKG_NAME}_${VERSION}_${ARCH}.deb"

# shellcheck source=metadata.sh
source "$PACKAGING_DIR/metadata.sh"

usage() {
    cat <<EOF
Uso: $(basename "$0") [opcoes]

  Compila o projeto em Release, instala com cmake --install e gera um .deb.

Opcoes:
  --skip-build    Nao recompila; usa o build existente
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
}

resolve_shlibs_depends() {
    local binary="$STAGING/usr/bin/${PKG_NAME}"
    local multiarch
    multiarch="$(dpkg-architecture -qDEB_HOST_MULTIARCH 2>/dev/null || true)"

    local -a library_paths=()
    if [[ -n "$multiarch" && -d "$STAGING/usr/lib/${multiarch}" ]]; then
        library_paths+=("-l${STAGING}/usr/lib/${multiarch}")
    fi
    if [[ -d "$STAGING/usr/lib" ]]; then
        library_paths+=("-l${STAGING}/usr/lib")
    fi

    local temp_dir
    temp_dir="$(mktemp -d)"
    mkdir -p "$temp_dir/debian"
    cat >"$temp_dir/debian/control" <<EOF
Source: ${PKG_NAME}
Package: ${PKG_NAME}
Version: ${VERSION}
Architecture: ${ARCH}
Maintainer: ${PKG_MAINTAINER}
EOF

    local shlibs_depends=""
    if shlibs_depends="$(
        cd "$temp_dir" && dpkg-shlibdeps -O "${library_paths[@]}" "$binary"
    )"; then
        rm -rf "$temp_dir"
        echo "${shlibs_depends#shlibs:Depends=}"
        return
    fi

    rm -rf "$temp_dir"
    echo "libc6 (>= 2.34), libgtk-4-1 (>= 4.14), libadwaita-1-0 (>= 1.5)"
}

write_control() {
    local shlibs_depends
    shlibs_depends="$(resolve_shlibs_depends)"

    cat >"$STAGING/DEBIAN/control" <<EOF
Package: ${PKG_NAME}
Version: ${VERSION}
Section: editors
Priority: optional
Architecture: ${ARCH}
Depends: ${shlibs_depends}
Maintainer: ${PKG_MAINTAINER}
Homepage: ${PKG_HOMEPAGE}
Description: ${PKG_SUMMARY}
$(echo "$PKG_DESCRIPTION" | sed 's/^/ /')
EOF
}

require_command cmake
require_command dpkg-deb
require_command dpkg
require_command dpkg-shlibdeps

if [[ ! -f "$ICON_SOURCE" ]]; then
    echo "Erro: icone de alta resolucao nao encontrado em $ICON_SOURCE" >&2
    exit 1
fi

CMAKE_ARGS=(
    -B "$BUILD_DIR"
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_INSTALL_PREFIX=/usr
    -DPLUMAS_DEVELOPER="${PKG_DEVELOPER}"
    -DPLUMAS_HOMEPAGE="${PKG_HOMEPAGE}"
    -DPLUMAS_SUMMARY="${PKG_SUMMARY}"
)

if [[ "$SKIP_BUILD" -eq 0 ]]; then
    echo "==> Configurando build Release..."
    cmake "${CMAKE_ARGS[@]}"
    echo "==> Compilando..."
    cmake --build "$BUILD_DIR" -j"$(nproc)"
fi

if [[ ! -x "$BUILD_DIR/plumas-editor-texto" ]]; then
    echo "Erro: binario nao encontrado em $BUILD_DIR/plumas-editor-texto" >&2
    exit 1
fi

echo "==> Gerando icones do tema hicolor..."
cmake --build "$BUILD_DIR" --target plumas_icons

echo "==> Instalando arquivos em staging com DESTDIR..."
rm -rf "$STAGING"
DESTDIR="$STAGING" cmake --install "$BUILD_DIR" --prefix /usr

mkdir -p "$STAGING/DEBIAN"
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
