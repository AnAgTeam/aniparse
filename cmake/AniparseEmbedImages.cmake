include_guard(GLOBAL)

# Compile encoded images into a target. FILES are explicit paths relative to ROOT
# and become lookup keys. HEADER declares the following function in NAMESPACE:
# std::optional<aniparse::EmbeddedImage> FUNCTION(std::string_view key);
# The generated translation unit stays in the build tree. Python is needed only
# by targets opting into embedding, and runs on the build host when cross compiling.
function(aniparse_embed_images)
    cmake_parse_arguments(PARSE_ARGV 0 EMBED "" "TARGET;ROOT;NAMESPACE;FUNCTION;HEADER" "FILES")
    if(EMBED_UNPARSED_ARGUMENTS OR EMBED_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR "Invalid aniparse_embed_images arguments")
    endif()
    foreach(required TARGET ROOT NAMESPACE FUNCTION HEADER FILES)
        if(NOT EMBED_${required})
            message(FATAL_ERROR "aniparse_embed_images requires ${required}")
        endif()
    endforeach()
    find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
    get_filename_component(root "${EMBED_ROOT}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    set(inputs)
    foreach(image IN LISTS EMBED_FILES)
        list(APPEND inputs "${root}/${image}")
    endforeach()
    string(MAKE_C_IDENTIFIER "${EMBED_NAMESPACE}_${EMBED_FUNCTION}" name)
    set(output "${CMAKE_CURRENT_BINARY_DIR}/embedded/${EMBED_TARGET}/${name}.cpp")
    set(generator "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/embed_images.py")
    add_custom_command(
        OUTPUT "${output}"
        COMMAND "${Python3_EXECUTABLE}" "${generator}"
            --root "${root}" --output "${output}"
            --namespace "${EMBED_NAMESPACE}" --function "${EMBED_FUNCTION}"
            --header "${EMBED_HEADER}" --files ${EMBED_FILES}
        DEPENDS "${generator}" ${inputs}
        COMMENT "Embedding images for ${EMBED_TARGET} (${EMBED_FUNCTION})"
        VERBATIM
    )
    target_sources(${EMBED_TARGET} PRIVATE "${output}")
endfunction()
