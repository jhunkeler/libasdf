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

libasdf's build system is built with the GNU autotools suite. To build this project
from source, you'll need the following software installed on your system:

Requirements
^^^^^^^^^^^^

To build this project from source, you'll need the following software installed
on your system:

- **GNU Autotools** (for generating the build system)
  
  - ``autoconf``
  - ``automake``
  - ``libtool`` (if your project uses it — remove if not)

- **C compiler** (e.g., ``gcc`` or ``clang``)
- **Make** (e.g., ``GNU make``)
- **pkg-config**

On **Debian/Ubuntu**::

    sudo apt install build-essential autoconf automake libtool pkg-config

On **Fedora**::

    sudo dnf install gcc make autoconf automake libtool pkgconf

On **macOS** (with Homebrew)::

    brew install autoconf automake libtool pkg-config

Building
^^^^^^^^

Clone the repository and build the project as follows::

    git clone https://github.com/asdf-format/libasdf.git
    cd libasdf
    ./autogen.sh
    ./configure
    make
    sudo make install   # Optional, installs the binary system-wide

Notes
^^^^^

- Run ``make clean`` to clean build artifacts.
- Run ``./configure --help`` to see available configuration options.
