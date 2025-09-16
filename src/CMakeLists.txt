project(libasdf)

add_library(libasdf STATIC)
target_link_libraries(libasdf PUBLIC stc 
	${FYAML_LIBRARIES}
	${ARGP_LIBRARIES}
)
target_include_directories(libasdf PUBLIC
        ${CMAKE_SOURCE_DIR}/third_party/STC/include
        ${CMAKE_BINARY_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_sources(libasdf PUBLIC
        types/asdf_common_tag_map.h
        types/asdf_extension_map.h
        core/asdf.c
        core/extension_metadata.c
        core/history_entry.c
        core/ndarray.c
        core/software.c
        block.c
        context.c
        error.c
        event.c
        extension_registry.c
        file.c
        info.c
        log.c
        parse.c
        parse_util.c
        stream.c
        value.c
        value_util.c
        yaml.c
)

add_executable(asdf main.c)
target_link_libraries(asdf libasdf stc)
target_include_directories(asdf PUBLIC
        ${STC_DIR}/include
        ${CMAKE_BINARY_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}
)

