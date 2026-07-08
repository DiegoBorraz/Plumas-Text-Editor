#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PACKAGING_DIR="$ROOT/packaging/windows"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-win}"

usage() {
    cat <<EOF
Uso: $(basename "$0") [opcoes]

  Compila no MSYS2 UCRT64, gera bundle GTK e instalador Inno Setup.

Opcoes:
  --skip-build    Nao recompila; usa build-win existente
  --skip-bundle   Nao recria staging; usa bundle existente
  -h, --help      Mostra esta ajuda
EOF
}

SKIP_BUILD=0
SKIP_BUNDLE=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --skip-build) SKIP_BUILD=1; shift ;;
        --skip-bundle) SKIP_BUNDLE=1; shift ;;
        -h | --help) usage; exit 0 ;;
        *) echo "Opcao desconhecida: $1" >&2; usage >&2; exit 1 ;;
    esac
done

if [[ -z "${MSYSTEM:-}" ]]; then
    echo "Erro: execute no shell MSYS2 UCRT64." >&2
    exit 1
fi

if [[ "$SKIP_BUILD" -eq 0 ]]; then
    echo "==> Configurando build Windows..."
    cmake -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release
    echo "==> Compilando..."
    cmake --build "$BUILD_DIR"
fi

if [[ "$SKIP_BUNDLE" -eq 0 ]]; then
    echo "==> Gerando bundle de runtime..."
    BUILD_DIR="$BUILD_DIR" "$PACKAGING_DIR/bundle-runtime.sh"
fi

if ! command -v iscc >/dev/null 2>&1; then
    echo "Erro: Inno Setup (iscc) nao encontrado no PATH." >&2
    exit 1
fi

mkdir -p "$PACKAGING_DIR/dist"
echo "==> Gerando instalador Inno Setup..."
(
    cd "$PACKAGING_DIR"
    iscc PlumasTextEditor.iss
)

echo ""
echo "Instalador criado em: $PACKAGING_DIR/dist/PlumasTextEditor-0.1.0-setup.exe"
