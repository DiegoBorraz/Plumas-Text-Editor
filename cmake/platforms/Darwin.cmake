option(PLUMAS_BUILD_MACOS "Enable macOS packaging helpers" OFF)

function(plumas_configure_platform_target target_name)
    if(PLUMAS_SANITIZER_FLAGS)
        target_link_options(${target_name} PUBLIC ${PLUMAS_SANITIZER_FLAGS})
    endif()
endfunction()

set(PLUMAS_PLATFORM_SOURCES
    src/platform/common/Paths.cpp
    src/platform/posix/PathsPosix.cpp
    src/platform/posix/FileBackend.cpp
)

add_library(plumas_platform ${PLUMAS_PLATFORM_SOURCES})
target_include_directories(plumas_platform PUBLIC include)
target_compile_options(plumas_platform PRIVATE ${PLUMAS_COMPILE_FLAGS} ${PLUMAS_SANITIZER_FLAGS})
target_compile_definitions(plumas_platform PRIVATE
    $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>
)
plumas_configure_platform_target(plumas_platform)

target_link_libraries(plumas_core PUBLIC plumas_platform)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(plumas-text-editor PRIVATE -Wl,-z,relro -Wl,-z,now -pie ${PLUMAS_SANITIZER_FLAGS})
endif()

if(PLUMAS_BUILD_MACOS)
    set_target_properties(plumas-text-editor PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.plumas.EditorTexto"
        MACOSX_BUNDLE_BUNDLE_NAME "Plumas Text Editor"
    )
endif()

install(TARGETS plumas-text-editor RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(
    FILES ${CMAKE_SOURCE_DIR}/packaging/debian/LICENSE
    DESTINATION share/doc/plumas-text-editor
)
