.. _opening:

Opening and reading ASDF files
==============================

The library's main handle to any given ASDF file is the opaque `asdf_file_t`
struct.  It is allocated, and a pointer to it returned (much like a `FILE *`
from stdlib functions like `open()`) using `asdf_open`:

.. code:: c

   asdf_file_t *file = asdf_open("observation.asdf", "r");

   if (file == NULL) {
       const char *error = asdf_error(file);
       fprintf(stderr, "error opening the ASDF file: %s\n", error);
       return 1;
   }

The second argument ``"r"`` is a mode string.  Currently ``"r"`` is the only
accepted value, but more will be added when write support is added.

In addition to `asdf_open`, `asdf_open_fp` can read from an existing `FILE *`,
and `asdf_open_mem` can read from a sized memory buffer.

.. _error-handling:

Error handling
--------------

Most state in libasdf, including the state of the parser, error messages, and
log messages, are tied to a single open file represented by an `asdf_file_t`.

Most functions, like `asdf_open`, that return a pointer to some kind of object
will return `NULL` if an error occurred, unless otherwise noted in the API
documentation.  In such cases, calling `asdf_error(file)` will return a message
explaining the error.

In the specific case of `asdf_open`, as an error result does not return an
`asdf_file_t *` in the first place, calling `asdf_error(NULL)` will return
global error messages.

`asdf_error` is used primarily for errors in the parsing/file structure itself,
as well as memory errors.  Other functions return a value of some error enum,
such as `asdf_value_err_t`.  This is used when reading values out of the file,
to indicate why reading the value failed (not found, wrong type, etc.). More on
this in :ref:`values`.

Closing the file and cleanup
----------------------------

Calling `asdf_close(file)` closes the underlying file handler (unless the file
was opened with `asdf_open_fp` in which case the caller is responsible for closing
the file), and releases data structures used by the ASDF parser.

It does *not* release memory used by other objects allocated in the process of
reading the file, `asdf_value_t`, `asdf_ndarray_t`, etc.  Those should be
cleaned up with calls to `asdf_value_destroy`, `asdf_ndarray_destroy`, and
other respective ``asdf_<type>_destroy`` calls when those objects are no longer
needed.

.. note::

   There are, however, plans to improve this by tracking references to
   resources associated with a single file, and releasing those resources,
   where possible, when closing the file.  This will make simple use cases a
   lot easier to manage.  Nevertheless, it's still good practice to release
   resources when no longer needed to avoid memory leaks in your own
   application.
