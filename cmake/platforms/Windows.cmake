function(plumas_configure_platform_target target_name)
    if(PLUMAS_SANITIZER_FLAGS)
        target_link_options(${target_name} PUBLIC ${PLUMAS_SANITIZER_FLAGS})
    endif()
endfunction()

set(PLUMAS_PLATFORM_SOURCES
    src/platform/common/Paths.cpp
    src/platform/win32/PathsWin32.cpp
    src/platform/win32/FileBackend.cpp
)

add_library(plumas_platform ${PLUMAS_PLATFORM_SOURCES})
target_include_directories(plumas_platform PUBLIC include)
target_compile_options(plumas_platform PRIVATE ${PLUMAS_COMPILE_FLAGS} ${PLUMAS_SANITIZER_FLAGS})
target_link_libraries(plumas_platform PUBLIC shell32)
plumas_configure_platform_target(plumas_platform)

target_link_libraries(plumas_core PUBLIC plumas_platform)

set(PLUMAS_ICON_SOURCE ${CMAKE_SOURCE_DIR}/plumas.png)
set(PLUMAS_ICON_FALLBACK ${CMAKE_SOURCE_DIR}/resources/plumas-icon.webp)

if(EXISTS "${PLUMAS_ICON_SOURCE}")
    set(PLUMAS_ICON_INPUT "${PLUMAS_ICON_SOURCE}")
elseif(EXISTS "${PLUMAS_ICON_FALLBACK}")
    set(PLUMAS_ICON_INPUT "${PLUMAS_ICON_FALLBACK}")
endif()

install(TARGETS plumas-text-editor RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(
    FILES ${CMAKE_SOURCE_DIR}/packaging/debian/LICENSE
    DESTINATION share/plumas-text-editor
)
