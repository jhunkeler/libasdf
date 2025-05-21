#pragma once

#include <string.h>


// TODO: Eventually move this to main public header
#if defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 4)
#define ASDF_EXPORT __attribute__ ((visibility ("default")))
#define ASDF_LOCAL __attribute__ ((visibility ("hidden")))
#else
#define ASDF_EXPORT
#define ASDF_LOCAL
#endif


#define FIELD_SIZEOF(t, f) (sizeof(((t*)0)->f))


/* Cast memset to volatile to prevent opt-away */ \
#define ZERO_MEMORY(ptr, size) \
    do { \
        void *volatile _volatile_ptr = (ptr); \
        memset(_volatile_ptr, 0, (size)); \
    } while (0)
