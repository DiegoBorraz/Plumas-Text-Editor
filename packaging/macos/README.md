# macOS packaging (stub)

Build manual com Homebrew:

```bash
brew install cmake ninja pkg-config gtk4 libadwaita nlohmann-json
cmake -B build-macos -G Ninja -DCMAKE_BUILD_TYPE=Release -DPLUMAS_BUILD_MACOS=ON
cmake --build build-macos
./build-macos/plumas-text-editor
```

Config: `~/Library/Application Support/plumas-text-editor/config.json`

## Futuro

- `bundle-app.sh` — gerar `Plumas Text Editor.app`
- `create-dmg.sh` — imagem DMG para distribuição
- CI em `macos-latest` quando houver runner Apple

O código POSIX em `src/platform/posix/` já cobre paths e File I/O no macOS.
