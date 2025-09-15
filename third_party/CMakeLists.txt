project(stc)
add_library(stc STATIC
        ${STC_DIR}/src/cregex.c
        ${STC_DIR}/src/cspan.c
        ${STC_DIR}/src/cstr_core.c
        ${STC_DIR}/src/cstr_io.c
        ${STC_DIR}/src/cstr_utf8.c
        ${STC_DIR}/src/csview.c
        ${STC_DIR}/src/fmt.c
        ${STC_DIR}/src/random.c
        ${STC_DIR}/src/singleheader.py
        ${STC_DIR}/src/singleupdate.sh
        ${STC_DIR}/src/stc_core.c
        ${STC_DIR}/src/utf8_tab.py
)
target_include_directories(stc PRIVATE
        STC/include
)
