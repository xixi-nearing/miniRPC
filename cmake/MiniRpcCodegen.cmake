function(mrpc_generate_cpp out_srcs out_hdrs)
    set(generated_srcs)
    set(generated_hdrs)

    foreach(idl IN LISTS ARGN)
        get_filename_component(base_name "${idl}" NAME_WE)
        set(header "${CMAKE_CURRENT_BINARY_DIR}/generated/${base_name}_rpc.h")
        set(source "${CMAKE_CURRENT_BINARY_DIR}/generated/${base_name}_rpc.cpp")

        add_custom_command(
            OUTPUT "${header}" "${source}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/mrpcgen.py"
                    --input "${idl}"
                    --header "${header}"
                    --source "${source}"
            DEPENDS "${idl}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/mrpcgen.py"
            COMMENT "Generating RPC bindings for ${base_name}"
            VERBATIM
        )

        list(APPEND generated_srcs "${source}")
        list(APPEND generated_hdrs "${header}")
    endforeach()

    set(${out_srcs} "${generated_srcs}" PARENT_SCOPE)
    set(${out_hdrs} "${generated_hdrs}" PARENT_SCOPE)
endfunction()
