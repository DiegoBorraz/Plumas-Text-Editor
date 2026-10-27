function(plumas_configure_platform_target target_name)
    if(PLUMAS_SANITIZER_FLAGS)
        target_link_options(${target_name} PUBLIC ${PLUMAS_SANITIZER_FLAGS})
    endif()
endfunction()

set(PLUMAS_PLATFORM_SOURCES
    src/platform/common/Paths.cpp
)

if(WIN32)
    list(APPEND PLUMAS_PLATFORM_SOURCES
        src/platform/win32/PathsWin32.cpp
        src/platform/win32/FileBackend.cpp
    )
else()
    list(APPEND PLUMAS_PLATFORM_SOURCES
        src/platform/posix/PathsPosix.cpp
        src/platform/posix/FileBackend.cpp
    )
endif()

add_library(plumas_platform ${PLUMAS_PLATFORM_SOURCES})
target_include_directories(plumas_platform PUBLIC include)
target_compile_options(plumas_platform PRIVATE ${PLUMAS_COMPILE_FLAGS} ${PLUMAS_SANITIZER_FLAGS})

if(WIN32)
    target_link_libraries(plumas_platform PUBLIC shell32)
endif()

plumas_configure_platform_target(plumas_platform)

target_link_libraries(plumas_core PUBLIC plumas_platform)

if(PLUMAS_SANITIZER_FLAGS)
    target_link_options(pluma-text PRIVATE ${PLUMAS_SANITIZER_FLAGS})
endif()

configure_file(
    ${CMAKE_SOURCE_DIR}/packaging/debian/com.plumatext.PlumaText.desktop.in
    ${CMAKE_CURRENT_BINARY_DIR}/com.plumatext.PlumaText.desktop
    @ONLY
)
configure_file(
    ${CMAKE_SOURCE_DIR}/packaging/debian/com.plumatext.PlumaText.metainfo.xml.in
    ${CMAKE_CURRENT_BINARY_DIR}/com.plumatext.PlumaText.metainfo.xml
    @ONLY
)

set(PLUMAS_ICON_SOURCE ${CMAKE_SOURCE_DIR}/plumas.png)
set(PLUMAS_ICON_FALLBACK ${CMAKE_SOURCE_DIR}/resources/plumas-icon.webp)
set(PLUMAS_ICON_NAME com.plumatext.PlumaText)
set(PLUMAS_ICON_SIZES 16 24 32 48 64 128 256 512)

if(EXISTS "${PLUMAS_ICON_SOURCE}")
    set(PLUMAS_ICON_INPUT "${PLUMAS_ICON_SOURCE}")
elseif(EXISTS "${PLUMAS_ICON_FALLBACK}")
    set(PLUMAS_ICON_INPUT "${PLUMAS_ICON_FALLBACK}")
    message(STATUS "plumas.png not found; using ${PLUMAS_ICON_FALLBACK} for hicolor icons")
endif()

if(DEFINED PLUMAS_ICON_INPUT)
    find_program(PLUMAS_MAGICK_COMMAND NAMES magick convert)

    foreach(size IN LISTS PLUMAS_ICON_SIZES)
        set(icon_output
            ${CMAKE_CURRENT_BINARY_DIR}/generated/icons/${size}x${size}/apps/${PLUMAS_ICON_NAME}.png)
        add_custom_command(
            OUTPUT ${icon_output}
            COMMAND ${CMAKE_COMMAND}
                -Dicon_source=${PLUMAS_ICON_INPUT}
                -Dsize=${size}
                -Doutput_path=${icon_output}
                -DPLUMAS_MAGICK_COMMAND=${PLUMAS_MAGICK_COMMAND}
                -DPython3_EXECUTABLE=${Python3_EXECUTABLE}
                -P ${CMAKE_SOURCE_DIR}/cmake/GenerateAppIcons.cmake
            DEPENDS ${PLUMAS_ICON_INPUT}
            COMMENT "Generating ${size}x${size} application icon"
        )
        list(APPEND PLUMAS_GENERATED_ICONS ${icon_output})
    endforeach()

    add_custom_target(plumas_icons DEPENDS ${PLUMAS_GENERATED_ICONS})
endif()

install(TARGETS pluma-text RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/com.plumatext.PlumaText.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/com.plumatext.PlumaText.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
)

install(
    FILES ${CMAKE_SOURCE_DIR}/packaging/debian/LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/pluma-text
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
