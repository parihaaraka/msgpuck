#ifndef EXT_TNT_H_INCLUDED
#define EXT_TNT_H_INCLUDED

#include <stdio.h>

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

typedef enum {
    MP_UNKNOWN_EXTENSION = 0,
    MP_DECIMAL = 1,
    MP_UUID = 2,
    MP_ERROR = 3,
    MP_DATETIME = 4,
    MP_COMPRESSION = 5,
    MP_INTERVAL = 6,
    mp_extension_type_MAX,
} mp_extension_type;

// https://github.com/tarantool/c-dt/blob/cec6acebb54d9e73ea0b99c63898732abd7683a6/dt_arithmetic.h
typedef enum {
    DT_EXCESS, // tnt excess
    DT_LIMIT,  // tnt none
    DT_SNAP    // tnt last
} dt_adjust_t;

// https://github.com/tarantool/tarantool/blob/master/src/lib/core/mp_interval.c
typedef enum {
    FIELD_YEAR = 0,
    FIELD_MONTH,
    FIELD_WEEK,
    FIELD_DAY,
    FIELD_HOUR,
    FIELD_MINUTE,
    FIELD_SECOND,
    FIELD_NANOSECOND,
    FIELD_ADJUST,
} interval_fields;

int
mp_fprint_ext_tnt(FILE *file, const char **data, int depth);
int
mp_snprint_ext_tnt(char *buf, int size, const char **data, int depth);
/**
 * @brief Encodes len bytes from src into dst.
 * @param dst Output buffer. Make sure it has len*2 bytes available.
 * @param src Raw bytes source.
 * @param len Number of source bytes to encode.
 */
void
hex_print(char **dst, const char **src, size_t len);

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */

#endif /* EXT_TNT_H_INCLUDED */
