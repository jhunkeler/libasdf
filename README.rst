libasdf
#######

.. _begin-badges:

.. image:: https://github.com/asdf-format/libasdf/workflows/Build/badge.svg
    :target: https://github.com/asdf-format/libasdf/actions
    :alt: CI Status

.. _end-badges:

C library for reading (and eventually writing) `ASDF
<https://www.asdf-format.org/en/latest/>`__ files


Introduction
============

libasdf is largely a wrapper around `libfyaml <https://pantoniou.github.io/libfyaml/>`__
but with an understanding of the structure of ASDF files, with the capability to read and
extract binary block data, as well as typed getters for metadata in the ASDF tree.

It also features an extension mechanism (still nascent) for reading ASDF schemas, including
the core schemas such as ``core/ndarray-<x.y.z>`` into C-native datastructures.

Getting Started
---------------

To open an ASDF file with libasdf the simplest way is to use the ``asdf_open`` function.
This returns an ``asdf_file_t *`` which is your main interface to the ASDF file.
When done with the file make sure to call ``asdf_close`` to free resources:

.. code:: c

   #include <stdio.h>
   #include <asdf.h>

   int main(void) {
       asdf_file_t *file = asdf_open("observation.asdf", "r");

       if (file == NULL) {
           fprintf(stderr, "error opening the ASDF file\n");
           return 1;
       }

       asdf_close(file);
       return 0;
    }

The following more complete example demonstrates how to read different metadata out of
the ASDF tree, as well as extract block data.  Inline comments provide further explanation:

.. code:: c

  #include <stdio.h>
  #include <asdf.h>

  int main(void) {
      // The mode string "r" is required and is the only currently-supported mode
      asdf_file_t *file = asdf_open("/path/to/file.asdf", "r");

      if (file == NULL) {
          fprintf(stderr, "error opening the ASDF file\n");
          return;
      }

      // The simplest way to read metadata from the file is with the
      // `asdf_get_<type>*` family of functions
      // They all return a value by pointer argument and return an
      // `asdf_value_error_t`
      // For example you can read a string from the metadata like:

      const char *software = NULL;
      // Returns a 0-terminated string into *software.
      asdf_value_err_t err = asdf_get_string0(file, "asdf_library/author", &software);

      if (err == ASDF_VALUE_OK) {
          fprintf("software: %s\n", software);
      }

      // Other errors could be e.g. ASDF_VALUE_ERR_NOT_FOUND if the key doesn't
      // exist, or ASDF_VALUE_ERR_TYPE_MISMATCH if it's not a string.

      // There are also extensions registered for some (not all yet) of the
      // core schemas.  Objects defined by extension schemas (identified by
      // their YAML tags) also have corresponding asdf_get_<type> functions:
      asdf_meta_t *meta = NULL;

      // This reads the top-level core/asdf-1.0.0 schema
      err = asdf_get_meta(file, "/", &meta);
      if (err == ASDF_VALUE_OK) {
          if (meta->history.entries[0]) {
              // This is a NULL-terminated array of asdf_history_entry_t*
              fprintf("first history entry: %s\n", meta->history.entries[0]->description);
          }
      }

      // Functions like `asdf_get_meta` that return into a double-pointer to a
      // struct allocate memory for that structure automatically.
      // The all have a corresponding `asdf_<type>_destroy` function.
      // The plan is to track these on the file object (issue #34) to make
      // memory management easier and cleaner, but for now you have to free
      // them manually when you're done with them. This is good practice in any
      // case.
      asdf_meta_destroy(meta);

      // ndarrays work no differently
      asdf_ndarray_t *ndarray = NULL;
      err = asdf_get_ndarray(file, "data", &ndarray);
      if (err != ASDF_VALUE_OK) {
          fprintf(stderr, "error reading ndarray data: %d\n", err);
          return;
      }

      printf("number of data dimensions: %d", ndarray->ndim);

      // Get just a raw pointer to the ndarray data block (if uncompressed).
      // Optionally returns the size in bytes as well
      size_t size = 0;
      void *data = asdf_ndarray_data_raw(ndarray, &size);

      // Slightly more useful is the asdf_ndarray_read_tile_ functions.
      // They can copy the data, including converting endianness into a tile
      // buffer.  If an existing buffer is not passed it will allocate one of
      // the correct size to hold the data.  The user is responsible for
      // freeing the buffer.

      // Read a 100x100x100 cube
      const uint64_t origin[3] = {0, 0, 0};
      const uint64_t shape[3] = {100, 100, 100};
      void *tile = NULL;
      asdf_ndarray_err_t array_err = asdf_ndarray_read_tile_ndim(
          ndarray,
          origin,
          shape,
          &tile
      );

      if (array_err != ASDF_NDARRAY_OK) {
          fprintf("error reading ndarray: %d\n", array_err);
      }

      free(tile);
      asdf_ndarray_destroy(ndarray);
      asdf_close(file);
      return 0;
  }


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
- **libfyaml**
- **argp** (this is a feature of glibc, but if compiling with a different libc you need a
  standalone version of this; also it is only needed if building the command-line tool)

On **Debian/Ubuntu**::

    sudo apt install build-essential autoconf automake libtool pkg-config libfyaml-dev

On **Fedora**::

    sudo dnf install gcc make autoconf automake libtool pkgconf libfyaml-devel

On **macOS** (with Homebrew)::

    brew install autoconf automake libtool pkg-config libfyaml argp-standalone

Building
^^^^^^^^

Clone the repository and build the project as follows::

    git clone https://github.com/asdf-format/libasdf.git
    cd libasdf
    ./autogen.sh
    ./configure
    make
    sudo make install   # Optional, installs the binary system-wide

If doing a system install, as usual it's recommended to install to ``/usr/local``
by providing ``--prefix=/usr/local`` when running ``./configure``.  Or, if you
have a ``${HOME}/.local`` you can set the prefix there, etc.

Notes
^^^^^

- Run ``make clean`` to clean build artifacts.
- Run ``./configure --help`` to see available configuration options.
