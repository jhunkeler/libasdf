#ifndef ASDF_UTIL_H
#define ASDF_UTIL_H

#if defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 4)
    #define ASDF_EXPORT __attribute__ ((visibility ("default")))
    #define ASDF_LOCAL __attribute__ ((visibility ("hidden")))
#else
    #define ASDF_EXPORT
    #define ASDF_LOCAL
#endif

#endif  /* ASDF_UTIL_H */
