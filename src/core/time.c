#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "asdf.h"
#include "time.h"

#include "../extension_registry.h"
#include "../log.h"
#include "../util.h"
#include "../value.h"
#include "stc/cregex.h"

#ifdef HAVE_STRPTIME
static const char *ASDF_TIME_SFMT_ISO_TIME[] = {"%Y-%m-%d %H:%M:%S", "%Y-%m-%d"};
static const char *ASDF_TIME_SFMT_YDAY[] = {"%Y:%j:%H:%M:%S", "%Y:%j"};
static const char *ASDF_TIME_SFMT_UNIX[] = {"%s"};

#define check_format_strptime(TYPE, BUF, TM, HAS_TIME, STATUS) \
    { \
        size_t idx = 0; \
        do { \
            (STATUS) = strptime((BUF), (TYPE)[idx], (TM)); \
            if ((STATUS)) { \
                (HAS_TIME) = true; \
                break; \
            } \
        } while (idx++ && idx < sizeof((TYPE)) / sizeof(*(TYPE))); \
    }

#define JD_B1900 2415020.31352
#define JD_MJD 2400000.5

/* Calendar constants */
static const double JD_GREGORIAN_START = 2299161.0;
static const double JD_CORRECTION_REF = 1867216.25;
static const double JD_CALENDAR_OFFSET = 122.1;
static const int JD_BASE_YEAR = 4716;
static const double JD_YEAR_LENGTH = 365.25;
static const double GREGORIAN_OFFSET = (int)1524.0;

static const double AVG_MONTH_LENGTH = 30.6001;
static const double AVG_YEAR_LENGTH = 365.242198781;
static const double DAYS_IN_CENTURY = 36524.2198781;
static const int SECONDS_PER_DAY = 86400;
static const int SECONDS_PER_HOUR = 3600;
static const int SECONDS_PER_MINUTE = 60;


/* Julian Date to Gregorian calendar conversion */
static void julian_to_tm(const double jd, struct tm *t, time_t *nanoseconds) {
    const double jd_shift = jd + 0.5;
    const int jd_int = (int)jd_shift;
    const double day_fraction = jd_shift - jd_int;

    int jd_adjust;
    if (jd_int < JD_GREGORIAN_START) {
        jd_adjust = jd_int;
    } else {
        const int leap_adjust = (int)((jd_int - JD_CORRECTION_REF) / DAYS_IN_CENTURY);
        jd_adjust = jd_int + 1 + leap_adjust - leap_adjust / 4;
    }

    const int calendar_day = jd_adjust + GREGORIAN_OFFSET;
    const int year_base = (int)((calendar_day - JD_CALENDAR_OFFSET) / JD_YEAR_LENGTH);
    const int days_in_years = (int)(JD_YEAR_LENGTH * year_base);
    const int month_base = (int)((calendar_day - days_in_years) / AVG_MONTH_LENGTH);

    const int day = calendar_day - days_in_years - (int)(AVG_MONTH_LENGTH * month_base) +
                    day_fraction;
    const int month = month_base < 14 ? month_base - 1 : month_base - 13;
    const int year = month > 2 ? year_base - JD_BASE_YEAR : year_base - JD_BASE_YEAR - 1;

    const double total_seconds = day_fraction * SECONDS_PER_DAY + 0.5;
    const int hour = (int)(total_seconds / SECONDS_PER_HOUR);
    const int minute = (int)((total_seconds - hour * SECONDS_PER_HOUR) / SECONDS_PER_MINUTE);
    const double seconds_whole = total_seconds - hour * SECONDS_PER_HOUR -
                                 minute * SECONDS_PER_MINUTE;
    const int second = (int)seconds_whole;
    const double fractional_seconds = seconds_whole - second;

    t->tm_year = year - 1900;
    t->tm_mon = month - 1;
    t->tm_mday = day;
    t->tm_hour = hour;
    t->tm_min = minute;
    t->tm_sec = second;

    if (nanoseconds) {
        *nanoseconds = (time_t)(fractional_seconds * 1e9) + 0.5;
    }
}


static void mjd_to_tm(const double mjd, struct tm *t, time_t *nsec) {
    const double jd = mjd + JD_MJD;
    julian_to_tm(jd, t, nsec);
}


static double besselian_to_julian(const double b) {
    return JD_B1900 + AVG_YEAR_LENGTH * (b - 1900.0);
}


int asdf_time_parse_std(
    const char *s, const asdf_time_format_t *format, struct asdf_time_info_t *out) {
    if (!s || !out) {
        return -1;
    }

    struct tm tm = {0};
    char tz_sign = 0;
    int tz_hour = 0;
    int tz_min = 0;
    long nsec = 0;
    bool has_time = false;
    char *rest = NULL;
    char *buf = strdup(s);

    if (!buf) {
        return -1;
    }

    /* Normalize separators (replace 'T' or 't' with space) */
    for (char *c = buf; *c; ++c) {
        if (*c == 'T' || *c == 't')
            *c = ' ';
    }

    switch (format->type) {
    case ASDF_TIME_FORMAT_DATETIME:
    case ASDF_TIME_FORMAT_ISO_TIME:
        check_format_strptime(ASDF_TIME_SFMT_ISO_TIME, buf, &tm, has_time, rest);
        break;
    case ASDF_TIME_FORMAT_YDAY:
        check_format_strptime(ASDF_TIME_SFMT_YDAY, buf, &tm, has_time, rest);
        break;
    case ASDF_TIME_FORMAT_UNIX:
        check_format_strptime(ASDF_TIME_SFMT_UNIX, buf, &tm, has_time, rest);
        break;
    default:
        free(buf);
        return -1;
    }

    if (!rest) {
        free(buf);
        return -1;
    }

    /* Handle optional fractional seconds */
    if (has_time) {
        const char *dot = strchr(rest, '.');
        if (dot) {
            double frac = 0;
            sscanf(dot, "%lf", &frac);
            nsec = (long)((frac - (int)frac) * 1e9);
        }

        /* Handle timezone offsets (Z/z = Zulu is ignored, just don't add any offset) */
        const char *tz = strpbrk(rest, "+-");
        if (tz && (*tz == '+' || *tz == '-')) {
            tz_sign = *tz == '-' ? -1 : 1;
            if (sscanf(tz + 1, "%2d:%2d", &tz_hour, &tz_min) < 1)
                sscanf(tz + 1, "%2d", &tz_hour);
        }
    }

    /* Convert to time_t and adjust for time zone */
    time_t t = timegm(&tm);
    if (t == (time_t)-1) {
        free(buf);
        return -1;
    }

    t -= tz_sign * (tz_hour * SECONDS_PER_HOUR + tz_min * SECONDS_PER_MINUTE);

    out->tm = *gmtime(&t);
    out->ts.tv_sec = t;
    out->ts.tv_nsec = nsec;
    free(buf);
    return 0;
}


static int asdf_time_parse_jd(const char *s, struct asdf_time_info_t *out) {
    const double jd = strtod(s, NULL);
    struct tm jd_tm;
    time_t nsec = 0;
    julian_to_tm(jd, &jd_tm, &nsec);
    const time_t t = timegm(&jd_tm);

    if (out) {
        out->tm = jd_tm;
        out->ts.tv_sec = t;
        out->ts.tv_nsec = nsec;
    } else {
        return -1;
    }
    return 0;
}


static int asdf_time_parse_mjd(const char *s, struct asdf_time_info_t *out) {
    const double mjd = strtod(s, NULL);
    struct tm mjd_tm;
    time_t nsec = 0;
    mjd_to_tm(mjd, &mjd_tm, &nsec);
    const time_t t = timegm(&mjd_tm);

    if (out) {
        out->tm = mjd_tm;
        out->ts.tv_sec = t;
        out->ts.tv_nsec = nsec;
    } else {
        return -1;
    }
    return 0;
}


int asdf_time_parse_byear(const char *s, struct asdf_time_info_t *out) {
    const double byear = strtod(s, NULL);
    const double jd = besselian_to_julian(byear);
    struct tm tm;
    time_t nsec = 0;

    julian_to_tm(jd, &tm, &nsec);
    const time_t t = timegm(&tm);

    if (out) {
        out->tm = *gmtime(&t);
        out->ts.tv_sec = t;
        out->ts.tv_nsec = nsec;
    } else {
        return -1;
    }
    return 0;
}


int asdf_time_parse_yday(const char *s, struct asdf_time_info_t *out) {
    const asdf_time_format_t fmt = {.is_base_format = true, .type = ASDF_TIME_FORMAT_YDAY};
    return asdf_time_parse_std(s, &fmt, out);
}

#else
#warning "strptime() not available, times will not be parsed"
static int asdf_time_parse_std(UNUSED(const char *s), struct timespec *out) {
    if (out) {
        out->tv_sec = 0;
        out->tv_nsec = 0;
    }
    return 0;
}
#endif


static int asdf_time_parse_time(
    const char *s, const asdf_time_format_t *format, struct asdf_time_info_t *out) {
    int status = -1;
    switch (format->type) {
    case ASDF_TIME_FORMAT_YDAY:
    case ASDF_TIME_FORMAT_ISO_TIME:
    case ASDF_TIME_FORMAT_DATETIME:
    case ASDF_TIME_FORMAT_UNIX:
        status = asdf_time_parse_std(s, format, out);
        break;
    case ASDF_TIME_FORMAT_MJD:
        status = asdf_time_parse_mjd(s, out);
        break;
    case ASDF_TIME_FORMAT_JD:
        status = asdf_time_parse_jd(s, out);
        break;
    case ASDF_TIME_FORMAT_BYEAR:
        status = asdf_time_parse_byear(s, out);
        break;
    default:
        break;
    }
    return status;
}


/*
 * Lookup table: asdf_time_base_format_t enum value -> YAML format name string
 *
 * Entries must be kept in the same order as asdf_time_base_format_t.
 */
static const char *const asdf_time_format_names[] = {
    "iso_time",    /* ASDF_TIME_FORMAT_ISO_TIME */
    "yday",        /* ASDF_TIME_FORMAT_YDAY */
    "byear",       /* ASDF_TIME_FORMAT_BYEAR */
    "jyear",       /* ASDF_TIME_FORMAT_JYEAR */
    "decimalyear", /* ASDF_TIME_FORMAT_DECIMALYEAR */
    "jd",          /* ASDF_TIME_FORMAT_JD */
    "mjd",         /* ASDF_TIME_FORMAT_MJD */
    "gps",         /* ASDF_TIME_FORMAT_GPS */
    "unix",        /* ASDF_TIME_FORMAT_UNIX */
    "utime",       /* ASDF_TIME_FORMAT_UTIME */
    "tai_seconds", /* ASDF_TIME_FORMAT_TAI_SECONDS */
    "cxcsec",      /* ASDF_TIME_FORMAT_CXCSEC */
    "galexsec",    /* ASDF_TIME_FORMAT_GALEXSEC */
    "unix_tai",    /* ASDF_TIME_FORMAT_UNIX_TAI */
    NULL,          /* ASDF_TIME_FORMAT_RESERVED1 */
    "byear_str",   /* ASDF_TIME_FORMAT_BYEAR_STR */
    "datetime",    /* ASDF_TIME_FORMAT_DATETIME */
    "fits",        /* ASDF_TIME_FORMAT_FITS */
    "isot",        /* ASDF_TIME_FORMAT_ISOT */
    "jyear_str",   /* ASDF_TIME_FORMAT_JYEAR_STR */
    "plot_date",   /* ASDF_TIME_FORMAT_PLOT_DATE */
    "ymdhms",      /* ASDF_TIME_FORMAT_YMDHMS */
    "datetime64",  /* ASDF_TIME_FORMAT_datetime64 */
};


/*
 * Lookup table: asdf_time_scale_t enum value -> YAML scale name string
 */
static const char *const asdf_time_scale_names[] = {
    "utc", /* ASDF_TIME_SCALE_UTC */
    "tai", /* ASDF_TIME_SCALE_TAI */
    "tcb", /* ASDF_TIME_SCALE_TCB */
    "tcg", /* ASDF_TIME_SCALE_TCG */
    "tdb", /* ASDF_TIME_SCALE_TDB */
    "tt",  /* ASDF_TIME_SCALE_TT */
    "ut1", /* ASDF_TIME_SCALE_UT1 */
};


static asdf_value_t *asdf_time_serialize(
    asdf_file_t *file, const void *obj, UNUSED(const void *userdata)) {

    if (UNLIKELY(!file || !obj))
        return NULL;

    const asdf_time_t *t = obj;
    asdf_mapping_t *map = NULL;
    asdf_value_t *value = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_EMIT_FAILURE;

    if (!t->value) {
        ASDF_LOG(file, ASDF_LOG_WARN, ASDF_CORE_TIME_TAG " requires a value");
        goto cleanup;
    }

    const size_t nformats = sizeof(asdf_time_format_names) / sizeof(asdf_time_format_names[0]);
    if ((size_t)t->format.type >= nformats || !asdf_time_format_names[t->format.type]) {
        ASDF_LOG(file, ASDF_LOG_WARN, ASDF_CORE_TIME_TAG " unknown or reserved format type");
        goto cleanup;
    }

    map = asdf_mapping_create(file);
    if (!map)
        goto cleanup;

    err = asdf_mapping_set_string0(map, "value", t->value);
    if (err != ASDF_VALUE_OK)
        goto cleanup;

    err = asdf_mapping_set_string0(map, "format", asdf_time_format_names[t->format.type]);
    if (err != ASDF_VALUE_OK)
        goto cleanup;

    /* Write scale only if non-UTC */
    if (t->scale != ASDF_TIME_SCALE_UTC) {
        const size_t nscales = sizeof(asdf_time_scale_names) / sizeof(asdf_time_scale_names[0]);
        if ((size_t)t->scale < nscales) {
            err = asdf_mapping_set_string0(map, "scale", asdf_time_scale_names[t->scale]);
            if (err != ASDF_VALUE_OK)
                goto cleanup;
        }
    }

    /* Write location only if any field is non-zero */
    if (t->location.longitude != 0.0 || t->location.latitude != 0.0 || t->location.height != 0.0) {
        asdf_mapping_t *loc_map = asdf_mapping_create(file);
        if (!loc_map)
            goto cleanup;

        err = asdf_mapping_set_double(loc_map, "longitude", t->location.longitude);
        if (err != ASDF_VALUE_OK) {
            asdf_mapping_destroy(loc_map);
            goto cleanup;
        }
        err = asdf_mapping_set_double(loc_map, "latitude", t->location.latitude);
        if (err != ASDF_VALUE_OK) {
            asdf_mapping_destroy(loc_map);
            goto cleanup;
        }
        err = asdf_mapping_set_double(loc_map, "height", t->location.height);
        if (err != ASDF_VALUE_OK) {
            asdf_mapping_destroy(loc_map);
            goto cleanup;
        }

        err = asdf_mapping_set_mapping(map, "location", loc_map);
        if (err != ASDF_VALUE_OK) {
            asdf_mapping_destroy(loc_map);
            goto cleanup;
        }
    }

    value = asdf_value_of_mapping(map);

cleanup:
    if (err != ASDF_VALUE_OK)
        asdf_mapping_destroy(map);

    return value;
}


static asdf_value_err_t asdf_time_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    const char *value_s = NULL;
    const char *format_s = NULL;

    asdf_mapping_t *mapping = NULL;
    asdf_value_t *prop = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;

    asdf_time_t *time = calloc(1, sizeof(asdf_time_t));
    if (!time) {
        return ASDF_VALUE_ERR_OOM;
    }

    if (asdf_value_is_mapping(value)) {
        if (asdf_value_as_mapping(value, &mapping) != ASDF_VALUE_OK)
            goto failure;
        prop = asdf_mapping_get(mapping, "value");
        if (!prop)
            goto failure;
    } else {
        prop = value;
    }

    time->value = calloc(ASDF_TIME_TIMESTR_MAXLEN, sizeof(*time->value));
    if (!time->value) {
        if (prop && prop != value)
            asdf_value_destroy(prop);
        free(time);
        return ASDF_VALUE_ERR_OOM;
    }

    const asdf_value_type_t type = asdf_value_get_type(prop);
    switch (type) {
    case ASDF_VALUE_INT64: {
        time_t value_tmp = 0;
        asdf_value_as_int64(value, &value_tmp);
        snprintf(time->value, ASDF_TIME_TIMESTR_MAXLEN, "%ld", value_tmp);
    } break;
    case ASDF_VALUE_DOUBLE: {
        double value_tmp = 0.0;
        asdf_value_as_double(value, &value_tmp);
        snprintf(time->value, ASDF_TIME_TIMESTR_MAXLEN, "%lf", value_tmp);
    } break;
    case ASDF_VALUE_FLOAT: {
        float value_tmp = 0.0f;
        asdf_value_as_float(value, &value_tmp);
        snprintf(time->value, ASDF_TIME_TIMESTR_MAXLEN, "%f", value_tmp);
    } break;
    case ASDF_VALUE_STRING: {
        asdf_value_as_string0(prop, &value_s);
        strncpy(time->value, value_s, ASDF_TIME_TIMESTR_MAXLEN - 1);
    } break;
    default:
        fprintf(stderr, "unhandled property conversion from scalar enum %d\n", prop->type);
        goto failure;
    }

    if (prop != value) {
        asdf_value_destroy(prop);
        prop = NULL;
    }

    if (mapping) {
        prop = asdf_mapping_get(mapping, "format");
        if (ASDF_VALUE_OK != asdf_value_as_string0(prop, &format_s)) {
            goto failure;
        }
        asdf_value_destroy(prop);
        prop = NULL;
    }

    const char *time_auto_keys[] = {
        "iso_time",
        "byear",
        "jyear",
        "yday",
    };

    const char *time_auto_patterns[] = {
        /* iso_time */
        "[0-9]{4}-(0[1-9])|(1[0-2])-(0[1-9])|([1-2][0-9])|(3[0-1])"
        "[T ]([0-1][0-9])|(2[0-4]):[0-5][0-9]:[0-5][0-9](.[0-9]+)?",
        /* byear */
        "B[0-9]+(.[0-9]+)?",
        /* jyear */
        "J[0-9]+(.[0-9]+)?",
        /* yday */
        /** FIXME: This regexp actually fails to compile due to surpassing the
         * built-in character class limit; need to find workaround to this */
        "[0-9]{4}:(00[1-9])|(0[1-9][0-9])|([1-2][0-9][0-9])|(3[0-5][0-9])|(36[0-5]):([0-1][0-9])|"
        "([0-1][0-9])|(2[0-4]):[0-5][0-9]:[0-5][0-9](.[0-9]+)?"};

    time->format.is_base_format = true;
    for (size_t idx = 0; idx < sizeof(time_auto_patterns) / sizeof(time_auto_patterns[0]); idx++) {
        cregex re = cregex_make(time_auto_patterns[idx], CREG_DEFAULT);

        if (UNLIKELY(re.error == CREG_OUTOFMEMORY)) {
            err = ASDF_VALUE_ERR_OOM;
            goto failure;
        } else if (UNLIKELY(re.error != CREG_OK)) {
            err = ASDF_VALUE_ERR_PARSE_FAILURE;
            goto failure;
        }

        if (cregex_is_match(&re, time->value) == true) {
            const char *fmt_have = format_s ? format_s : time_auto_keys[idx];
            if (!strcmp(fmt_have, "iso_time")) {
                time->format.type = ASDF_TIME_FORMAT_ISO_TIME;
            } else if (!strcmp(fmt_have, "byear") || !strncmp(time->value, "B", 1)) {
                time->format.type = ASDF_TIME_FORMAT_BYEAR;
            } else if (!strcmp(fmt_have, "jd")) {
                time->format.type = ASDF_TIME_FORMAT_JD;
            } else if (!strcmp(fmt_have, "mjd")) {
                time->format.type = ASDF_TIME_FORMAT_MJD;
            } else if (!strcmp(fmt_have, "jyear") || !strncmp(time->value, "J", 1)) {
                time->format.type = ASDF_TIME_FORMAT_JYEAR;
            } else if (!strcmp(fmt_have, "yday")) {
                time->format.type = ASDF_TIME_FORMAT_YDAY;
            } else if (!strcmp(fmt_have, "unix")) {
                time->format.type = ASDF_TIME_FORMAT_UNIX;
            }
            time->scale = ASDF_TIME_SCALE_UTC;
            cregex_drop(&re);
            break;
        }
        cregex_drop(&re);
    }

    if (time->format.type > ASDF_TIME_FORMAT_RESERVED1) {
        time->format.is_base_format = false;
    }

    asdf_time_parse_time(time->value, &time->format, &time->info);

    *out = time;

    return ASDF_VALUE_OK;
failure:
    free(time->value);
    free(time);
    asdf_value_destroy(prop);
    return err;
}


static void *asdf_time_copy(const void *obj) {
    if (!obj)
        return NULL;

    const asdf_time_t *t = obj;
    asdf_time_t *copy = calloc(1, sizeof(asdf_time_t));

    if (!copy)
        return NULL;

    *copy = *t;
    copy->value = t->value ? strdup(t->value) : NULL;

    return copy;
}


static void asdf_time_dealloc(void *value) {
    asdf_time_t *t = (asdf_time_t *)value;
    free(t->value);
    free(t);
}


ASDF_REGISTER_EXTENSION(
    time,
    ASDF_CORE_TIME_TAG,
    asdf_time_t,
    &libasdf_software,
    asdf_time_serialize,
    asdf_time_deserialize,
    asdf_time_copy,
    asdf_time_dealloc,
    NULL);
