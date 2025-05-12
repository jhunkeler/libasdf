#pragma once

#include <string.h>


#define FIELD_SIZEOF(t, f) (sizeof(((t*)0)->f))


/* Cast memset to volatile to prevent opt-away */ \
#define ZERO_MEMORY(ptr, size) \
    do { \
        void *volatile _volatile_ptr = (ptr); \
        memset(_volatile_ptr, 0, (size)); \
    } while (0)
