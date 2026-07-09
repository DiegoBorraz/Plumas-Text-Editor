# Plumas Text Editor

Editor de texto leve multiplataforma (Linux e Windows; macOS em preparação), escrito em C++20 com GTK4 e libadwaita. Focado em abrir, editar e salvar arquivos de texto comuns, com interface em inglês, números de linha, busca/substituição e empacotamento `.deb` (Ubuntu/Debian) e instalador `.exe` (Windows).

**Homepage:** [https://github.com/DiegoBorraz/Plumas-Text-Editor](https://github.com/DiegoBorraz/Plumas-Text-Editor)

## Funcionalidades

- Abertura e salvamento de arquivos de texto com whitelist de extensões
- Novo documento, salvar, salvar como e confirmação de alterações não salvas
- Números de linha desenhados com Cairo/Pango (sem inflar a altura da janela)
- Busca e substituição com **Ctrl+F** (Find, Replace, Replace All, Previous/Next)
- Abertura de arquivos pela linha de comando ou “Abrir com” (`G_APPLICATION_HANDLES_OPEN`)
- Tema claro/escuro via libadwaita
- Lista de arquivos recentes em `~/.config/plumas-text-editor/config.json`
- Limite de documento de 50 MB
- Recursos embutidos via GResource (CSS e ícone na barra de título)
- Ícones do menu de aplicativos (tema hicolor) gerados a partir de `plumas.png`

## Ícones e recursos visuais

O projeto usa **dois arquivos de ícone**, com papéis diferentes:

| Arquivo | Local | Uso |
|---------|-------|-----|
| `plumas.png` | Raiz do repositório | Fonte **512×512** para gerar os PNGs do tema hicolor (menu do Ubuntu, `.desktop`, AppStream) |
| `resources/plumas-icon.webp` | `resources/` | Ícone embutido na **barra de título** via GResource |

Ambos fazem parte do repositório. O `plumas.png` foi derivado do WebP para servir de fonte em alta resolução no empacotamento.

Se `plumas.png` não existir, o build usa `resources/plumas-icon.webp` como fallback (`CMakeLists.txt` e `packaging/debian/build-deb.sh`).

Para regenerar o PNG a partir do WebP:

```bash
python3 - <<'PY'
from PIL import Image
img = Image.open("resources/plumas-icon.webp").convert("RGBA")
img.resize((512, 512), Image.Resampling.LANCZOS).save("plumas.png", format="PNG")
print("plumas.png atualizado")
PY
```

Requer `python3-pil` (`sudo apt install python3-pil`) ou ImageMagick (`magick`).

## Tecnologias

| Camada | Tecnologia |
|--------|------------|
| Linguagem | C++20 |
| UI | GTK4, libadwaita |
| Build | CMake 3.20+, GNUInstallDirs |
| Configuração | nlohmann/json |
| Recursos | GResource (`glib-compile-resources`) |
| Empacotamento | `.deb` (Debian/Ubuntu) + instalador Inno Setup (Windows) |
| CI | GitHub Actions (Linux + Windows + ASan/UBSan) |

### Dependências de sistema (Ubuntu/Debian)

**Obrigatórias** (compilar e empacotar):

```bash
sudo apt install -y \
  build-essential cmake pkg-config \
  libgtk-4-dev libadwaita-1-dev libglib2.0-dev \
  nlohmann-json3-dev dpkg-dev
```

**Opcionais** (geração dos ícones hicolor — uma das duas basta):

```bash
sudo apt install -y imagemagick
# ou
sudo apt install -y python3-pil
```

Os ícones do menu são gerados em tempo de build a partir de `plumas.png` (ou do WebP, em fallback), via `cmake/GenerateAppIcons.cmake`.

## Estrutura do projeto

```
plumas-editor-texto/
├── plumas.png                   # Ícone fonte 512×512 (tema hicolor / .deb)
├── src/
│   ├── main.cpp                 # Ponto de entrada
│   ├── core/                    # Lógica de domínio (sem GTK)
│   │   ├── Document.cpp         # Estado do documento (conteúdo, path, dirty)
│   │   ├── FileIO.cpp           # Leitura/escrita segura de arquivos
│   │   └── Config.cpp           # Persistência JSON (arquivos recentes)
│   └── ui/                      # Camada de apresentação GTK
│       ├── Application.cpp      # AdwApplication, startup, open-with
│       ├── MainWindow.cpp       # Layout, toolbar, resize, atalhos
│       ├── EditorView.cpp       # GtkTextView, gutter, limites de tamanho
│       ├── SearchReplace.cpp    # Busca/substituição (thread em Replace All)
│       ├── Dialogs.cpp          # Diálogos abrir/salvar/confirmações
│       ├── TitleBar.cpp         # Barra de título customizada
│       ├── Resources.cpp        # GResource e fallback em XDG_DATA_DIRS
│       ├── UiHelpers.cpp        # Toast, status, Pango nos labels
│       └── styles.css           # Estilos da interface
├── include/plumas/
│   ├── core/                    # Headers do núcleo
│   └── ui/                      # Headers da UI e AppState
├── resources/
│   ├── plumas.gresource.xml     # Bundle GResource (CSS + ícone webp)
│   └── plumas-icon.webp         # Ícone da barra de título (embutido no binário)
├── packaging/
│   ├── debian/                  # Instalador .deb (Linux)
│   └── windows/                 # Bundle GTK + Inno Setup (.exe)
├── cmake/
│   ├── GenerateAppIcons.cmake   # Redimensiona plumas.png → ícones hicolor
│   └── platforms/               # Linux.cmake, Windows.cmake, Darwin.cmake
├── .github/workflows/ci.yml     # Pipeline de CI
└── CMakeLists.txt
```

## Padrão de projeto e arquitetura

O código segue uma separação em **duas camadas**:

1. **`plumas::core`** — domínio puro: documento, I/O de arquivos e configuração. Não depende de GTK.
2. **`plumas::platform`** — paths, I/O POSIX/Win32 e detecção de chrome nativo da janela.
3. **`plumas::ui`** — interface: janelas, widgets, sinais GTK e estado visual (`AppState`).

### Convenções adotadas

- **Namespaces** por módulo (`plumas::core`, `plumas::platform`, `plumas::ui`)
- **Headers públicos** em `include/plumas/...`, implementação em `src/...`
- **Estado da aplicação** centralizado em `AppState` (ponteiros GTK + `Document` + `Config`)
- **Funções livres** na camada UI para ações (`tryOpenPath`, `saveDocument`, `refreshEditor`) em vez de classes widget gigantes
- **CMake** com bibliotecas estáticas `plumas_core`, `plumas_platform` e `plumas_ui`; regras por SO em `cmake/platforms/`
- **Instalação** via `cmake --install` com `GNUInstallDirs` (FHS: `/usr/bin`, `/usr/share/...`)

### Boas práticas aplicadas

- C++20 com extensões desabilitadas (`CMAKE_CXX_EXTENSIONS OFF`)
- Compilação defensiva: `-Wall -Wextra -Wpedantic`, `-fstack-protector-strong`, `_FORTIFY_SOURCE=2`, PIE e `relro/now` no link
- I/O com APIs POSIX ou Win32 via `plumas::platform` (symlinks, tamanho, escrita atômica)
- Escrita atômica de arquivos (temporário + `fsync` + `rename`)
- Recursos embutidos com GResource para instalação reproduzível
- Config em `XDG_CONFIG_HOME` (Linux), `%APPDATA%` (Windows) ou `~/Library/Application Support` (macOS)
- `.gitignore` para artefatos de build, staging e pacotes
- CI com job dedicado a AddressSanitizer e UndefinedBehaviorSanitizer (`-DPLUMAS_ENABLE_SANITIZERS=ON`)

## Medidas de segurança

| Área | Medida |
|------|--------|
| Leitura de arquivos | `O_RDONLY \| O_NOFOLLOW \| O_CLOEXEC`; rejeita symlinks e não-arquivos regulares |
| Escrita de arquivos | Bloqueia destino symlink; escrita atômica via `mkstemp` + `rename` |
| Paths | Normalização (`weakly_canonical`), limite de 4096 caracteres, sem `\0` |
| Extensões | Whitelist (`.txt`, `.md`, `.py`, `.sh`, etc.) em abrir e salvar |
| Tamanho | Máx. 50 MB por documento; config JSON máx. 1 MB |
| Config | Leitura com `O_NOFOLLOW`; `chmod 600` ao salvar; paths recentes absolutos e sem symlink |
| Ícone | Validação de tamanho (máx. 512 KB) antes de carregar WebP do disco ou GResource |
| Replace All | Execução em thread separada com cancelamento; limite de 1 milhão de substituições |
| Editor | Bloqueio de inserção além do limite de caracteres; verificação antes de salvar |
| Build | Sanitizers opcionais para detectar memory leaks e UB em desenvolvimento/CI |

O editor **não executa** scripts, não interpreta XML/HTML e não abre rede — superfície de ataque limitada a I/O local e dependências do sistema (GTK, Pango, nlohmann/json).

## Executar em desenvolvimento

Na raiz do repositório:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/plumas-text-editor
```

Abrir um arquivo pela CLI:

```bash
./build/plumas-text-editor /caminho/para/arquivo.txt
```

Atalhos úteis na interface:

- **Ctrl+F** — busca e substituição
- **Esc** — fecha a barra de busca

Build com sanitizers (depuração):

```bash
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DPLUMAS_ENABLE_SANITIZERS=ON
cmake --build build-asan
./build-asan/plumas-text-editor
```

## Build e instalação no Ubuntu

### 1. Gerar o instalador `.deb`

```bash
./packaging/debian/build-deb.sh
```

O script:

1. Configura e compila em **Release**
2. Gera ícones do tema hicolor a partir de `plumas.png` (fallback: `resources/plumas-icon.webp`)
3. Instala os arquivos em staging com `DESTDIR`
4. Monta o pacote com `dpkg-deb`

Opção para reutilizar build existente (só empacotar):

```bash
./packaging/debian/build-deb.sh --skip-build
```

### 2. Instalar ou reinstalar

```bash
sudo apt install --reinstall ./packaging/debian/dist/plumas-text-editor_0.1.0_amd64.deb
```

Ajuste o nome do arquivo se a versão ou arquitetura forem diferentes:

```bash
ls packaging/debian/dist/
```

### 3. Executar a versão instalada

```bash
plumas-text-editor
```

Ou pelo menu de aplicativos: **Plumas Text Editor**.

### Instalador — local e nome

| Item | Valor |
|------|--------|
| **Pasta** | `packaging/debian/dist/` |
| **Nome do pacote** | `plumas-text-editor_0.1.0_amd64.deb` |
| **Comando no terminal** | `plumas-text-editor` |
| **Binário instalado** | `/usr/bin/plumas-text-editor` |
| **Desktop entry** | `/usr/share/applications/plumas-text-editor.desktop` |
| **Ícones do menu** | `/usr/share/icons/hicolor/*/apps/plumas-text-editor.png` |
| **App ID** | `com.plumas.EditorTexto` |
| **Versão** | Definida em `CMakeLists.txt` (`project(... VERSION 0.1.0)`) |

### Desinstalar

```bash
sudo apt remove plumas-text-editor
```

## Build e instalador no Windows

Requisitos no [MSYS2](https://www.msys2.org/) (shell **UCRT64**):

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-gtk4 \
  mingw-w64-ucrt-x86_64-libadwaita \
  mingw-w64-ucrt-x86_64-nlohmann-json \
  mingw-w64-ucrt-x86_64-pkg-config
```

Para gerar o instalador `.exe`, instale também o [Inno Setup](https://jrsoftware.org/isinfo.php) e deixe `iscc` no `PATH`.

```bash
./packaging/windows/build-installer.sh
```

O script compila em `build-win/`, monta o bundle GTK em `packaging/windows/staging/` e gera:

| Item | Valor |
|------|--------|
| **Pasta** | `packaging/windows/dist/` |
| **Instalador** | `PlumasTextEditor-0.1.0-setup.exe` |
| **Config do usuário** | `%APPDATA%\plumas-text-editor\config.json` |

Checklist de QA manual: [`packaging/windows/QA.md`](packaging/windows/QA.md).

No Windows a janela usa **decorations nativas** do sistema; no Linux mantém barra de título customizada.

## Licença

Distribuído sob a licença **Plumas-Free-Non-Commercial-1.0** (uso livre, sem monetização). Texto completo em [`LICENSE`](LICENSE) e [`packaging/debian/LICENSE`](packaging/debian/LICENSE).

## Autor

Diego Avila — [diego.avila.dev@gmail.com](mailto:diego.avila.dev@gmail.com)
