cmake_minimum_required(VERSION 3.21)
project(asdf C)

# Global includes
include(GNUInstallDirs)
include(FindPkgConfig)
include(CheckSymbolExists)
include(CheckFunctionExists)
include(CheckSourceRuns)
include(CheckSourceRuns)
include(CheckIncludeFile)

# Local includes
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(SubmoduleInit)

set(ASDFSTD_DIR ${CMAKE_SOURCE_DIR}/asdf-standard)
set(STC_DIR ${CMAKE_SOURCE_DIR}/third_party/STC)
set(MUNIT_DIR ${CMAKE_SOURCE_DIR}/tests/munit)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(PACKAGE_NAME "asdf")
set(PACKAGE_VERSION "0.0.0")

add_compile_options(-fvisibility=hidden)

# Configure config.h
add_compile_definitions(HAVE_CONFIG_H)
add_compile_definitions(__USE_MISC)
add_compile_definitions(_XOPEN_SOURCE)
add_compile_definitions(_GNU_SOURCE)
if(APPLE)
    # for time.h: timegm
    add_compile_definitions(_DARWIN_C_SOURCE)
endif()

check_include_file(endian.h HAVE_ENDIAN_H)
check_include_file(machine/endian.h HAVE_MACHINE_ENDIAN_H)
check_include_file(sys/endian.h HAVE_SYS_ENDIAN_H)
if(HAVE_ENDIAN_H)
    set(ENDIAN_H endian.h)
elseif(HAVE_MACHINE_ENDIAN_H)
    set(ENDIAN_H machine/endian.h)
elseif(HAVE_SYS_ENDIAN_H)
    set(ENDIAN_H sys/endian.h)
endif()

check_function_exists(strptime HAVE_STRPTIME)
check_source_runs(C "
    #include <stdio.h>
    #include <${ENDIAN_H}>
    int main() {
    #ifdef be64toh
        puts(\"YES\");
        return 0;
    #else
        puts(\"NO\");
        return 1;
    #endif
    }
    "
    HAVE_DECL_BE64TOH)



# Update git submodules
list(APPEND submodules
    asdf-standard
    third_party/STC
    tests/munit
)
submodule_init(${submodules})

# Configure options for ASDF library
if(ASDF_LOG_ENABLED)
    option(ASDF_LOG_COLOR "Enable colored log output" ON)
endif()
set(ASDF_DEFAULT_LOG_LEVEL TRACE CACHE STRING "One of TRACE|DEBUG|INFO|WARN|ERROR|FATAL|NONE")
set(ASDF_LOG_MIN_LEVEL TRACE CACHE STRING "One of TRACE|DEBUG|INFO|WARN|ERROR|FATAL|NONE")

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

if(APPLE)
    option(ARGP_NO_PKGCONFIG NO)
    if(ARGP_NO_PKGCONFIG)
        set(ARGP_LIBRARIES "argp")
        set(ARGP_LIBDIR "" CACHE STRING "Directory containing libargp library")
        set(ARGP_INCLUDEDIR "" CACHE STRING "Directory containing libargp headers")
        set(ARGP_CFLAGS "" CACHE STRING "Compiler options for libargp")
        set(ARGP_LDFLAGS "" CACHE STRING "Linker options for libargp")
        link_directories(${ARGP_LIBDIR})
        include_directories(${ARGP_INCLUDEDIR})
        add_link_options(${ARGP_LDFLAGS})
        add_compile_options(${ARGP_CFLAGS})
    else()
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(ARGP libargp REQUIRED)
        else()
            message("pkg-config not found. Install pkg-config, or use ARGP_NO_PKGCONFIG=YES.")
        endif()
    endif()
endif()

include_directories(${CMAKE_SOURCE_DIR}/include)
configure_file(config.h.cmake include/config.h @ONLY)

add_subdirectory(third_party)
add_subdirectory(src)
add_subdirectory(include)

# Testing
option(ENABLE_TESTING OFF)
option(ENABLE_TESTING_SHELL OFF)

if(ENABLE_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

