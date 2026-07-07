cmake_minimum_required(VERSION 3.20)
project(plumas-editor-texto VERSION 0.1.0 LANGUAGES CXX)

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

add_library(plumas_core
    src/core/Document.cpp
    src/core/Config.cpp
    src/core/FileIO.cpp
)
target_include_directories(plumas_core PUBLIC include)
target_link_libraries(plumas_core PUBLIC nlohmann_json::nlohmann_json)
target_compile_options(plumas_core PRIVATE ${PLUMAS_COMPILE_FLAGS})
target_compile_definitions(plumas_core PRIVATE $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>)

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
    src/ui/UiHelpers.cpp
    src/ui/Resources.cpp
    ${PLUMAS_GRESOURCE_C}
)
target_include_directories(plumas_ui PUBLIC include)
target_link_libraries(plumas_ui PUBLIC plumas_core PkgConfig::GTK4 PkgConfig::ADW PkgConfig::GIO)
target_compile_options(plumas_ui PRIVATE ${PLUMAS_COMPILE_FLAGS})
target_compile_definitions(plumas_ui PRIVATE
    $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>
    PLUMAS_INSTALL_DATADIR="${CMAKE_INSTALL_FULL_DATADIR}"
)

add_executable(plumas-editor-texto src/main.cpp)
target_link_libraries(plumas-editor-texto PRIVATE plumas_ui)
target_compile_options(plumas-editor-texto PRIVATE ${PLUMAS_COMPILE_FLAGS})
target_compile_definitions(plumas-editor-texto PRIVATE $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(plumas-editor-texto PRIVATE -Wl,-z,relro -Wl,-z,now -pie)
endif()

configure_file(
    ${CMAKE_SOURCE_DIR}/packaging/debian/plumas-editor-texto.desktop.in
    ${CMAKE_CURRENT_BINARY_DIR}/plumas-editor-texto.desktop
    @ONLY
)
configure_file(
    ${CMAKE_SOURCE_DIR}/packaging/debian/com.plumas.EditorTexto.metainfo.xml.in
    ${CMAKE_CURRENT_BINARY_DIR}/com.plumas.EditorTexto.metainfo.xml
    @ONLY
)

set(PLUMAS_ICON_SOURCE ${CMAKE_SOURCE_DIR}/plumas.png)
set(PLUMAS_ICON_NAME plumas-editor-texto)
set(PLUMAS_ICON_SIZES 16 24 32 48 64 128 256 512)

if(EXISTS "${PLUMAS_ICON_SOURCE}")
    find_program(PLUMAS_MAGICK_COMMAND NAMES magick convert)

    foreach(size IN LISTS PLUMAS_ICON_SIZES)
        set(icon_output
            ${CMAKE_CURRENT_BINARY_DIR}/generated/icons/${size}x${size}/apps/${PLUMAS_ICON_NAME}.png)
        add_custom_command(
            OUTPUT ${icon_output}
            COMMAND ${CMAKE_COMMAND}
                -Dicon_source=${PLUMAS_ICON_SOURCE}
                -Dsize=${size}
                -Doutput_path=${icon_output}
                -DPLUMAS_MAGICK_COMMAND=${PLUMAS_MAGICK_COMMAND}
                -DPython3_EXECUTABLE=${Python3_EXECUTABLE}
                -P ${CMAKE_SOURCE_DIR}/cmake/GenerateAppIcons.cmake
            DEPENDS ${PLUMAS_ICON_SOURCE}
            COMMENT "Generating ${size}x${size} application icon"
        )
        list(APPEND PLUMAS_GENERATED_ICONS ${icon_output})
    endforeach()

    add_custom_target(plumas_icons DEPENDS ${PLUMAS_GENERATED_ICONS})
endif()

install(TARGETS plumas-editor-texto RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/plumas-editor-texto.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/com.plumas.EditorTexto.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
)

install(
    FILES ${CMAKE_SOURCE_DIR}/packaging/debian/LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/plumas-editor-texto
)

if(TARGET plumas_icons)
    install(CODE "
        execute_process(
            COMMAND \"${CMAKE_COMMAND}\" --build \"${CMAKE_BINARY_DIR}\" --target plumas_icons
            RESULT_VARIABLE _plumas_icon_build_result
        )
        if(NOT _plumas_icon_build_result EQUAL 0)
            message(FATAL_ERROR \"Failed to generate application icons\")
        endif()
    ")
    foreach(size IN LISTS PLUMAS_ICON_SIZES)
        install(
            FILES ${CMAKE_CURRENT_BINARY_DIR}/generated/icons/${size}x${size}/apps/${PLUMAS_ICON_NAME}.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps
        )
    endforeach()
endif()

include(CPack)
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_CONTACT "${PLUMAS_DEVELOPER}")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PLUMAS_HOMEPAGE}")
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/packaging/debian/LICENSE)
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_SECTION "editors")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
