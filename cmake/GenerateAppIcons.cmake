if(NOT DEFINED icon_source OR NOT DEFINED size OR NOT DEFINED output_path)
    message(FATAL_ERROR "icon_source, size and output_path are required")
endif()

if(NOT DEFINED Python3_EXECUTABLE)
    message(FATAL_ERROR "Python3_EXECUTABLE is required")
endif()

get_filename_component(output_dir "${output_path}" DIRECTORY)
file(MAKE_DIRECTORY "${output_dir}")

if(DEFINED PLUMAS_MAGICK_COMMAND AND NOT PLUMAS_MAGICK_COMMAND STREQUAL "PLUMAS_MAGICK_COMMAND-NOTFOUND")
    execute_process(
        COMMAND "${PLUMAS_MAGICK_COMMAND}" "${icon_source}"
            -resize "${size}x${size}"
            "${output_path}"
        RESULT_VARIABLE resize_result
    )
    if(resize_result EQUAL 0)
        return()
    endif()
endif()

execute_process(
    COMMAND "${Python3_EXECUTABLE}" -c
        "from PIL import Image; import sys; source, size_text, output = sys.argv[1:4]; size = int(size_text); image = Image.open(source); image.convert('RGBA').resize((size, size), Image.Resampling.LANCZOS).save(output, format='PNG')"
        "${icon_source}"
        "${size}"
        "${output_path}"
    RESULT_VARIABLE python_result
    ERROR_VARIABLE python_error
)

if(NOT python_result EQUAL 0)
    message(FATAL_ERROR "Failed to generate ${output_path}: ${python_error}")
endif()
