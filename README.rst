libasdf
#######

C library for reading (and eventually writing) `ASDF
<https://www.asdf-format.org/en/latest/>`__ files


Introduction
============

Let's have something to say here first.


Development
===========

Minimal requirements
--------------------

First we'll have to have some (probably libfyaml + headers)


Building from source tarballs
-----------------------------

First we'd have to have some.


Building from git
-----------------

libasdf's build system is built with CMake. To build this project
from source, you'll need the following software installed on your system:

Requirements
^^^^^^^^^^^^

To build this project from source, you'll need the following software installed
on your system:

- **CMake** (for generating the build system)
- **C compiler** (e.g., ``gcc`` or ``clang``)
- **Make** (e.g., ``GNU make``)
- **pkg-config**
- **libfyaml**
- **argp** (this is a feature of glibc, but if compiling with a different libc you need a
  standalone version of this; also it is only needed if building the command-line tool)

On **Debian/Ubuntu**::

    sudo apt install build-essential pkg-config libfyaml-dev

On **Fedora**::

    sudo dnf install gcc make pkgconf libfyaml-devel

On **macOS** (with Homebrew)::

    brew install pkg-config libfyaml argp-standalone

Building
^^^^^^^^

Clone the repository and build the project as follows::

    git clone https://github.com/asdf-format/libasdf.git
    cd libasdf
    mkdir build
    cd build
    cmake .. \
        -D ENABLE_TESTING=[YES/NO] \
        -D ENABLE_TESTING_SHELL=[YES/NO] \
        -D ENABLE_ASAN=[YES/NO] \
        -D FYAML_NO_PKGCONFIG=[YES/NO] \
            # If YES \
            -D FYAML_LIBDIR=[path/lib] \
            -D FYAML_INCLUDEDIR=[path/include] \
        -D ARGP_NO_PKGCONFIG=[YES/NO] \
            # If YES \
            -D ARGP_LIBDIR=[path/lib] \
            -D ARGP_INCLUDEDIR=[path/include]
    make
    sudo make install   # Optional, installs the binary system-wide

If doing a system install, as usual it's recommended to install to ``/usr/local``
by providing ``-DCMAKE_INSTALL_PREFIX=/usr/local`` when running ``cmake``.  Or, if you
have a ``${HOME}/.local`` you can set the prefix there, etc.

Notes
^^^^^

- Run ``make clean`` to clean build artifacts.
- Run ``make project_source`` to generate a source archive with CPack
- Run ``ctest --output-on-failure`` to execute unit tests
