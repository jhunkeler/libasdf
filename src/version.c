#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "asdf/version.h"
#include "util.h"


asdf_version_t *asdf_version_parse(const char *version) {
    if (UNLIKELY(!version))
        return NULL;

    asdf_version_t *ver = calloc(1, sizeof(asdf_version_t));

    if (UNLIKELY(!ver))
        return NULL;

    ver->version = strdup(version);

    if (UNLIKELY(!ver->version))
        goto fail;

    const char *chr = version;
    char *end = NULL;

    if (!isdigit(*chr)) // Not a semver; early return
        return ver;

    // NOLINTNEXTLINE(readability-magic-numbers)
    ver->major = strtoul(chr, &end, 10);

    if (*end != '.') {
        if (*end != '\0') {
            ver->extra = strdup(end);

            if (UNLIKELY(!ver->extra))
                goto fail;
        }

        return ver;
    }

    chr = end + 1;
    if (!isdigit(*chr)) {
        ver->extra = strdup(chr);

        if (UNLIKELY(!ver->extra))
            goto fail;

        return ver;
    }

    // NOLINTNEXTLINE(readability-magic-numbers)
    ver->minor = strtoul(chr, &end, 10);

    if (*end != '.') {
        if (*end != '\0') {
            ver->extra = strdup(end);

            if (UNLIKELY(!ver->extra))
                goto fail;
        }

        return ver;
    }

    chr = end + 1;
    if (!isdigit(*chr)) {
        ver->extra = strdup(chr);

        if (UNLIKELY(!ver->extra))
            goto fail;

        return ver;
    }

    // NOLINTNEXTLINE(readability-magic-numbers)
    ver->patch = strtoul(chr, &end, 10);
    chr = end;

    if (*chr != '\0') {
        if (*chr == '.' || *chr == '-')
            chr++;

        if (*chr != '\0') {
            ver->extra = strdup(chr);

            if (UNLIKELY(!ver->extra))
                goto fail;
        }
    }

    return ver;
fail:
    asdf_version_destroy(ver);
    return NULL;
}


void asdf_version_destroy(asdf_version_t *version) {
    if (!version)
        return;

    free((void *)version->version);
    free((void *)version->extra);
    ZERO_MEMORY(version, sizeof(asdf_version_t));
    free(version);
}
