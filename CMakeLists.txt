cmake_minimum_required(VERSION 3.20)
project(plumas-editor-texto VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(nlohmann_json 3.2.0 REQUIRED)

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

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)
pkg_check_modules(ADW REQUIRED IMPORTED_TARGET libadwaita-1)

add_library(plumas_ui
    src/ui/Application.cpp
    src/ui/MainWindow.cpp
    src/ui/EditorView.cpp
    src/ui/TitleBar.cpp
    src/ui/Dialogs.cpp
    src/ui/UiHelpers.cpp
    src/ui/Resources.cpp
)
target_include_directories(plumas_ui PUBLIC include)
target_link_libraries(plumas_ui PUBLIC plumas_core PkgConfig::GTK4 PkgConfig::ADW)
target_compile_options(plumas_ui PRIVATE ${PLUMAS_COMPILE_FLAGS})
target_compile_definitions(plumas_ui PRIVATE $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>)

add_executable(plumas-editor-texto src/main.cpp)
target_link_libraries(plumas-editor-texto PRIVATE plumas_ui)
target_compile_options(plumas-editor-texto PRIVATE ${PLUMAS_COMPILE_FLAGS})
target_compile_definitions(plumas-editor-texto PRIVATE $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(plumas-editor-texto PRIVATE -Wl,-z,relro -Wl,-z,now -pie)
endif()

set(PLUMAS_RESOURCES_DIR ${CMAKE_BINARY_DIR}/resources)

add_custom_command(
    OUTPUT ${PLUMAS_RESOURCES_DIR}/styles.css
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PLUMAS_RESOURCES_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/src/ui/styles.css
        ${PLUMAS_RESOURCES_DIR}/styles.css
    DEPENDS ${CMAKE_SOURCE_DIR}/src/ui/styles.css
    COMMENT "Copiando styles.css"
)

add_custom_target(plumas_styles ALL DEPENDS ${PLUMAS_RESOURCES_DIR}/styles.css)
add_dependencies(plumas-editor-texto plumas_styles)

add_custom_command(
    TARGET plumas-editor-texto POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:plumas-editor-texto>/resources
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/resources/plumas-icon.webp
        $<TARGET_FILE_DIR:plumas-editor-texto>/resources/plumas-icon.webp
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/src/ui/styles.css
        $<TARGET_FILE_DIR:plumas-editor-texto>/resources/styles.css
    COMMENT "Copiando recursos da aplicacao"
)
