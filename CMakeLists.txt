cmake_minimum_required(VERSION 3.20)
project(plumas-text-editor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(GNUInstallDirs)

set(PLUMAS_DEVELOPER "Diego Avila" CACHE STRING "Developer name shown in desktop and AppStream metadata")
set(PLUMAS_HOMEPAGE "https://github.com/DiegoBorraz/Plumas-Text-Editor" CACHE STRING "Project homepage")
set(PLUMAS_SUMMARY "Lightweight GTK4 text editor" CACHE STRING "Short package summary")

find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_program(GLIB_COMPILE_RESOURCES glib-compile-resources REQUIRED)

pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)
pkg_check_modules(ADW REQUIRED IMPORTED_TARGET libadwaita-1)
pkg_check_modules(GIO REQUIRED IMPORTED_TARGET gio-2.0)

set(PLUMAS_COMPILE_FLAGS
    -Wall
    -Wextra
    -Wpedantic
    -fstack-protector-strong
    -Wformat
    -Wformat-security
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    list(APPEND PLUMAS_COMPILE_FLAGS -fPIE)
endif()

set(PLUMAS_SANITIZER_FLAGS "")

option(PLUMAS_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(PLUMAS_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        list(APPEND PLUMAS_SANITIZER_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer)
    else()
        message(WARNING "Sanitizers are only supported with GCC or Clang.")
    endif()
endif()

add_library(plumas_core
    src/core/Document.cpp
    src/core/Config.cpp
    src/core/FileIO.cpp
)
target_include_directories(plumas_core PUBLIC include)
target_link_libraries(plumas_core PUBLIC nlohmann_json::nlohmann_json)
target_compile_options(plumas_core PRIVATE ${PLUMAS_COMPILE_FLAGS} ${PLUMAS_SANITIZER_FLAGS})
target_compile_definitions(plumas_core PRIVATE $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>)
if(PLUMAS_SANITIZER_FLAGS)
    target_link_options(plumas_core PUBLIC ${PLUMAS_SANITIZER_FLAGS})
endif()

set(PLUMAS_GRESOURCE_XML ${CMAKE_SOURCE_DIR}/resources/plumas.gresource.xml)
set(PLUMAS_GRESOURCE_C ${CMAKE_CURRENT_BINARY_DIR}/plumas-resources.c)

add_custom_command(
    OUTPUT ${PLUMAS_GRESOURCE_C}
    COMMAND ${GLIB_COMPILE_RESOURCES}
        --target=${PLUMAS_GRESOURCE_C}
        --generate-source
        --sourcedir=${CMAKE_SOURCE_DIR}/resources
        ${PLUMAS_GRESOURCE_XML}
    DEPENDS
        ${PLUMAS_GRESOURCE_XML}
        ${CMAKE_SOURCE_DIR}/src/ui/styles.css
        ${CMAKE_SOURCE_DIR}/resources/plumas-icon.webp
    COMMENT "Compiling GResource bundle"
)

add_library(plumas_ui
    src/ui/Application.cpp
    src/ui/MainWindow.cpp
    src/ui/EditorView.cpp
    src/ui/TitleBar.cpp
    src/ui/Dialogs.cpp
    src/ui/SearchReplace.cpp
    src/ui/UiHelpers.cpp
    src/ui/Resources.cpp
    ${PLUMAS_GRESOURCE_C}
)
target_include_directories(plumas_ui PUBLIC include)
target_link_libraries(plumas_ui PUBLIC plumas_core PkgConfig::GTK4 PkgConfig::ADW PkgConfig::GIO)
target_compile_options(plumas_ui PRIVATE ${PLUMAS_COMPILE_FLAGS} ${PLUMAS_SANITIZER_FLAGS})
target_compile_definitions(plumas_ui PRIVATE
    $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>
    PLUMAS_INSTALL_DATADIR="${CMAKE_INSTALL_FULL_DATADIR}"
)

add_executable(plumas-text-editor src/main.cpp)
target_link_libraries(plumas-text-editor PRIVATE plumas_ui)
target_compile_options(plumas-text-editor PRIVATE ${PLUMAS_COMPILE_FLAGS} ${PLUMAS_SANITIZER_FLAGS})
target_compile_definitions(plumas-text-editor PRIVATE $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>)

if(WIN32)
    include(${CMAKE_SOURCE_DIR}/cmake/platforms/Windows.cmake)
elseif(APPLE)
    include(${CMAKE_SOURCE_DIR}/cmake/platforms/Darwin.cmake)
else()
    include(${CMAKE_SOURCE_DIR}/cmake/platforms/Linux.cmake)
endif()
