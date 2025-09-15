cmake_minimum_required(VERSION 3.21)
project(asdf C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CheckSymbolExists)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(FindPkgConfig)
include(SubmoduleInit)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(PACKAGE_NAME "asdf")
set(PACKAGE_VERSION "0.0.0")
list(APPEND submodules
        asdf-standard
        third_party/STC
)

option(ASDF_LOG_ENABLED "Enable logging" ON)
if(ASDF_LOG_ENABLED)
    option(ASDF_LOG_COLOR "Enable colored log output" ON)
endif()
set(ASDF_DEFAULT_LOG_LEVEL TRACE CACHE STRING "One of TRACE|DEBUG|INFO|WARN|ERROR|FATAL|NONE")
set(ASDF_LOG_MIN_LEVEL TRACE CACHE STRING "One of TRACE|DEBUG|INFO|WARN|ERROR|FATAL|NONE")

submodule_init(submodules)

option(FYAML_NO_PKGCONFIG NO)

if(FYAML_NO_PKGCONFIG)
    set(FYAML_LIBRARIES "fyaml")
    set(FYAML_LIBDIR "" CACHE STRING "Directory containing libfyaml library")
    set(FYAML_INCLUDEDIR "" CACHE STRING "Directory containing libfyaml headers")
    set(FYAML_CFLAGS "" CACHE STRING "Compiler options for libfyaml")
    set(FYAML_LDFLAGS "" CACHE STRING "Linker options for libfyaml")
    link_directories(${FYAML_LIBDIR})
    include_directories(${FYAML_INCLUDEDIR})
    add_link_options(${FYAML_LDFLAGS})
    add_compile_options(${FYAML_CFLAGS})
else()
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(FYAML libfyaml REQUIRED)
    else()
        message("pkg-config not found. Install pkg-config, or use FYAML_NO_PKGCONFIG=YES.")
    endif()
endif()

set(STC_DIR ${CMAKE_SOURCE_DIR}/third_party/STC)
add_compile_definitions(HAVE_CONFIG_H)
add_compile_definitions(_XOPEN_SOURCE_EXTENDED)
add_compile_definitions(_GNU_SOURCE)
add_compile_definitions(_POSIX_SOURCE)

check_include_file(endian.h HAVE_ENDIAN_H)
check_include_file(machine/endian.h HAVE_MACHINE_ENDIAN_H)
check_include_file(sys/endian.h HAVE_SYS_ENDIAN_H)
check_function_exists(strptime HAVE_STRPTIME)
check_symbol_exists(be64toh "endian.h" HAVE_DECL_BE64TOH)

include_directories(${CMAKE_SOURCE_DIR}/include)
configure_file(config.h.cmake include/config.h @ONLY)

add_subdirectory(third_party)
add_subdirectory(src)
