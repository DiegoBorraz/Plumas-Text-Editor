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
# windows.h would otherwise define min()/max() macros that break std::min/max.
target_compile_definitions(plumas_platform PRIVATE NOMINMAX)
if(MINGW)
    # MinGW keeps __stack_chk_fail and the _FORTIFY_SOURCE helpers in libssp,
    # which GCC links only when the stack protector flag reaches the linker.
    target_link_options(plumas_platform PUBLIC -fstack-protector-strong)
endif()
plumas_configure_platform_target(plumas_platform)

target_link_libraries(plumas_core PUBLIC plumas_platform)

set_target_properties(pluma-text PROPERTIES WIN32_EXECUTABLE TRUE)

install(TARGETS pluma-text RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(
    FILES ${CMAKE_SOURCE_DIR}/packaging/debian/LICENSE
    DESTINATION share/pluma-text
)
