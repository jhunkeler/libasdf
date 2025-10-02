add_executable(asdf_dump_key asdf_dump_key.c)
target_include_directories(asdf_dump_key PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${BZIP2_INCLUDEDIR}
    ${LZ4_INCLUDEDIR}
    ${ZLIB_INCLUDEDIR}
)
target_link_libraries(asdf_dump_key
    libasdf
    m
    ${BZIP2_LIBRARIES}
    ${LZ4_LIBRARIES}
    ${ZLIB_LIBRARIES}
)

