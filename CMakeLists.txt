cmake_minimum_required(VERSION 3.20)
project(pluma-text VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# _FORTIFY_SOURCE does nothing without optimization, so never build unoptimized
# by accident.
get_property(PLUMAS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT PLUMAS_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

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
    list(APPEND PLUMAS_COMPILE_FLAGS
        -fPIE
        -fstack-clash-protection
        # Bounds checks in std::string/vector operator[]; negligible cost here.
        -D_GLIBCXX_ASSERTIONS
        # Some toolchains predefine level 3; -U avoids a redefinition clash.
        $<$<CONFIG:Release,RelWithDebInfo,MinSizeRel>:-U_FORTIFY_SOURCE>
        $<$<CONFIG:Release,RelWithDebInfo,MinSizeRel>:-D_FORTIFY_SOURCE=3>
    )
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i.86")
        list(APPEND PLUMAS_COMPILE_FLAGS -fcf-protection=full)
    endif()
endif()

set(PLUMAS_LINK_FLAGS "")
if(NOT WIN32 AND NOT APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    list(APPEND PLUMAS_LINK_FLAGS -pie -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack)
endif()

set(PLUMAS_SANITIZER_FLAGS "")

# libFuzzer targets in tests/fuzz (Clang only). Every target is built with
# coverage instrumentation so the fuzzer can see into plumas_core.
option(PLUMAS_BUILD_FUZZERS "Build the libFuzzer targets (requires Clang)" OFF)
if(PLUMAS_BUILD_FUZZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "PLUMAS_BUILD_FUZZERS requires Clang (CXX=clang++).")
    endif()
    add_compile_options(-fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
endif()

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
        ${CMAKE_SOURCE_DIR}/src/ui/styles-dark.css
        ${CMAKE_SOURCE_DIR}/src/ui/styles-adw-variables.css
        ${CMAKE_SOURCE_DIR}/resources/plumas-icon.webp
    COMMENT "Compiling GResource bundle"
)

add_library(plumas_ui
    src/ui/Application.cpp
    src/ui/MainWindow.cpp
    src/ui/MenuBar.cpp
    src/ui/EditorView.cpp
    src/ui/TitleBar.cpp
    src/ui/Dialogs.cpp
    src/ui/SearchReplace.cpp
    src/ui/Tabs.cpp
    src/ui/UiHelpers.cpp
    src/ui/Resources.cpp
)
target_include_directories(plumas_ui PUBLIC include)
target_link_libraries(plumas_ui PUBLIC plumas_core PkgConfig::GTK4 PkgConfig::ADW PkgConfig::GIO)
target_compile_options(plumas_ui PRIVATE ${PLUMAS_COMPILE_FLAGS} ${PLUMAS_SANITIZER_FLAGS})
target_compile_definitions(plumas_ui PRIVATE
    PLUMAS_INSTALL_DATADIR="${CMAKE_INSTALL_FULL_DATADIR}"
)

# The generated GResource registers itself from a constructor; linking it
# directly into the executable keeps the linker from dropping it.
add_executable(pluma-text src/main.cpp ${PLUMAS_GRESOURCE_C})
target_link_libraries(pluma-text PRIVATE plumas_ui)
target_link_options(pluma-text PRIVATE ${PLUMAS_LINK_FLAGS})
target_compile_options(pluma-text PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:${PLUMAS_COMPILE_FLAGS}>
    ${PLUMAS_SANITIZER_FLAGS})

if(WIN32)
    include(${CMAKE_SOURCE_DIR}/cmake/platforms/Windows.cmake)
elseif(APPLE)
    include(${CMAKE_SOURCE_DIR}/cmake/platforms/Darwin.cmake)
else()
    include(${CMAKE_SOURCE_DIR}/cmake/platforms/Linux.cmake)
endif()

option(PLUMAS_BUILD_TESTS "Build the core unit tests" ON)
if(PLUMAS_BUILD_TESTS)
    enable_testing()
    add_executable(plumas_core_tests tests/core_tests.cpp)
    target_link_libraries(plumas_core_tests PRIVATE plumas_core)
    target_compile_options(plumas_core_tests PRIVATE ${PLUMAS_COMPILE_FLAGS} ${PLUMAS_SANITIZER_FLAGS})
    add_test(NAME plumas_core_tests COMMAND plumas_core_tests)
endif()

if(PLUMAS_BUILD_FUZZERS)
    foreach(fuzzer fuzz_text fuzz_config)
        add_executable(${fuzzer} tests/fuzz/${fuzzer}.cpp)
        target_link_libraries(${fuzzer} PRIVATE plumas_core PkgConfig::GIO)
        target_link_options(${fuzzer} PRIVATE -fsanitize=fuzzer)
    endforeach()
endif()
