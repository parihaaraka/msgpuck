#include "msgpuck.h"

enum mp_extension_type {
	MP_UNKNOWN_EXTENSION = 0,
	MP_DECIMAL = 1,
	MP_UUID = 2,
	MP_ERROR = 3,
	MP_DATETIME = 4,
	MP_COMPRESSION = 5,
	MP_INTERVAL = 6,
	mp_extension_type_MAX,
};

int
print_decimal(char **buf, size_t buf_size, const char *val, uint32_t val_bytes)
{
	int64_t scale = 0;
	const char *scale_head = val;
	enum mp_type scale_mp_type = mp_typeof(*val);
	if (scale_mp_type == MP_UINT)
		scale = (int64_t)mp_decode_uint(&val);
	else if (scale_mp_type == MP_INT)
		scale = mp_decode_int(&val);

	if (scale_head == val || !val_bytes)
		return -1;         /* "undefined" (is ext mp type 1 misused?) */

	val_bytes -= val - scale_head;

	char *pos = *buf;
	int output_length = 0;
	uint8_t sign_nibble = (uint8_t)(val[val_bytes - 1]) & 0xF;
	if (sign_nibble == 0xB || sign_nibble == 0xD) {
		++output_length;
		if (buf_size > 1) /* there is a reason to do something */
			*pos++ = '-';
	}

	uint32_t nibbles_left = val_bytes * 2 - 1;
	if (!(*val & 0xF0))   /* first nibble is 0 */
		--nibbles_left;

	if (!nibbles_left) {  /* e.g. 0e2 */
		scale = 0;
		output_length = 1;
		pos = *buf;       /* in case of negative */
	} else if (scale >= nibbles_left) {
		output_length += (size_t)scale + 2; /* 0. */
	} else if (scale > 0) {
		output_length += nibbles_left + 1;  /*  . */
	} else {
		output_length += nibbles_left + (size_t)(-scale);
	}

	if ((size_t)output_length > buf_size)
		return output_length;

	char *first_dec_digit = pos;
	*first_dec_digit = 'N';   /* no output mark */

	if (scale > 0 && scale >= nibbles_left) {
		*pos++ = '0';
		*pos++ = '.';
		for (int i = 0; i < scale - nibbles_left; ++i)
			*pos++ = '0';
	}

	uint8_t nz_flag = 0;
	do {
		uint8_t c = (uint8_t)(*(val++));
		if (nibbles_left & 0x1) {
			*pos = (char)('0' + (c >> 4));
			nz_flag |= *pos++;
			--nibbles_left;
			if (scale && nibbles_left == scale)
				*pos++ = '.';
		}

		if (nibbles_left) {
			*pos = (char)('0' + (c & 0xF));
			nz_flag |= *pos++;
			--nibbles_left;
			if (scale && nibbles_left == scale)
				*pos++ = '.';
		}
	} while (nibbles_left);

	if (nz_flag) {                        /* non-zero digit exists */
		while (scale++ < 0)
			*pos++ = '0';
	} else if (*first_dec_digit == 'N') { /* no digits at all */
		*first_dec_digit = '0';
		pos = first_dec_digit + 1;
	}

	int len = (pos - *buf);
	assert(len == output_length);
	*buf = pos;
	return len;
}

void
hex_print(char **dst, const char **src, size_t len)
{
	const char hex[] = "0123456789abcdef";
	char *pos = *dst;
	*dst += len << 1;
	while (len-- > 0) {
		*pos++ = hex[(**src>>4) & 0xF];
		*pos++ = hex[**src & 0xF];
		++*src;
	}
}

int
print_uuid(char **buf, size_t buf_size, const char *val, uint32_t val_bytes)
{
	if (val_bytes != 16)
		return -1;
	int res_length = 38;
	if (buf_size >= (size_t)res_length)	{
		*(*buf)++ = '"';
		hex_print(buf, &val, 4);
		*(*buf)++ = '-';
		hex_print(buf, &val, 2);
		*(*buf)++ = '-';
		hex_print(buf, &val, 2);
		*(*buf)++ = '-';
		hex_print(buf, &val, 2);
		*(*buf)++ = '-';
		hex_print(buf, &val, 6);
		*(*buf)++ = '"';
	}
	return res_length;
}

#define PRINT(FN, ...) \
{ \
	int res = FN(*buf, buf_size, __VA_ARGS__); \
	if (mp_unlikely(res < 0)) \
	return res; \
	if ((size_t)res < buf_size) {*buf += res; buf_size -= res;} \
	else {*buf = NULL; buf_size = 0;} \
	total_length += res; \
}

int
print_error_stack(char **buf, size_t buf_size, const char *val, uint32_t val_bytes)
{
	(void)val_bytes;
	int total_length = 0;

#define PRINT_HEX() \
	{ \
		const char *start = val; \
		mp_next(&val); \
		int src_len = val - start; \
		int dst_len = src_len << 1; \
		if (*buf && buf_size > (size_t)dst_len) { \
			hex_print(buf, &start, src_len); \
			buf_size -= dst_len; \
		} else { \
			*buf = NULL; \
			buf_size = 0; \
		} \
		total_length += dst_len; \
	}

	PRINT(snprintf, "{")
	uint32_t ext_payload_count = mp_decode_map(&val);
	for (uint32_t map0item = 0; map0item < ext_payload_count; ++map0item) {
		if (map0item)
			PRINT(snprintf, ", ")
		uint64_t key = mp_decode_uint(&val);
		if (key == 0) { // MP_ERROR_STACK
			PRINT(snprintf, "\"stack\": [")
			uint32_t stack_count = mp_decode_array(&val);
			for (uint32_t array1item = 0; array1item < stack_count; ++array1item) {
				PRINT(snprintf, (array1item > 0 ? ", {" : "{"))
				uint32_t error_fields_count = mp_decode_map(&val);
				int prev_length = total_length;
				while (error_fields_count-- > 0) {
					if (total_length > prev_length)
						PRINT(snprintf, ", ")
					uint64_t key = mp_decode_uint(&val);
					switch(key) {
					case 0: PRINT(snprintf, "\"type\": "); PRINT(mp_snprint, val); mp_next(&val); break;
					case 1: PRINT(snprintf, "\"file\": "); PRINT(mp_snprint, val); mp_next(&val); break;
					case 2: PRINT(snprintf, "\"line\": "); PRINT(mp_snprint, val); mp_next(&val); break;
					case 3: PRINT(snprintf, "\"message\": "); PRINT(mp_snprint, val); mp_next(&val); break;
					case 4: {
						uint64_t errno = mp_decode_uint(&val);
						if (errno)
							PRINT(snprintf, "\"errno\": %ld", errno)
						else if (total_length > prev_length) {
							if (*buf) {*buf -= 2; buf_size -= 2;}
							total_length -= 2;
						}
						break;
					}
					case 5: {
						uint64_t code = mp_decode_uint(&val);
						if (code)
							PRINT(snprintf, "\"code\": %ld", code)
						else if (total_length > prev_length) {
							if (*buf) {*buf -= 2; buf_size -= 2;}
							total_length -= 2;
						}
						break;
					}
					case 6:
						PRINT(snprintf, "\"fields\": "); PRINT(mp_snprint, val); mp_next(&val); break;
					default: // print unknown keys in hex
						PRINT(snprintf, "\"%lu\": \"", key)
						PRINT_HEX()
						PRINT(snprintf, "\"")
					}
				}
				PRINT(snprintf, "}")
			}
			PRINT(snprintf, "]")
		} else { // print unknown keys in hex
			PRINT(snprintf, "\"%lu\": \"", key)
			PRINT_HEX()
			PRINT(snprintf, "\"")
		}
	}
	PRINT(snprintf, "}")

	return total_length;
}

#define PRINT_UNIX_TIME(epoch, nsec) \
{ \
	PRINT(snprintf, "%ld", epoch); \
	if (nsec != 0) { \
		if (nsec % 1000000 == 0) { \
			PRINT(snprintf, ".%03d", nsec / 1000000); \
		} else if (nsec % 1000 == 0) { \
			PRINT(snprintf, ".%06d", nsec / 1000); \
		} else { \
			PRINT(snprintf, ".%09d", nsec); \
		} \
	} \
}

int
print_datetime(char **buf, size_t buf_size, const char *val, uint32_t val_bytes)
{
	// print the datetime as unix time (fp value)

	struct tmp {
		int32_t nsec;
		int16_t tzoffset;
		int16_t tzindex;
	} tail = {0, 0, 0};

	if (val_bytes != sizeof(int64_t) && val_bytes != sizeof(int64_t) + sizeof(tail))
		return -1;

	int total_length = 0;
	int64_t epoch;
	memcpy(&epoch, val, sizeof(epoch));
	val += sizeof(epoch);

	if (val_bytes != sizeof(int64_t))
		memcpy(&tail, val, sizeof(tail));

	PRINT_UNIX_TIME(epoch, tail.nsec);

	return total_length;
}

int
print_interval(char **buf, size_t buf_size, const char *val, uint32_t val_bytes)
{
	(void)val_bytes;

	// https://github.com/tarantool/c-dt/blob/cec6acebb54d9e73ea0b99c63898732abd7683a6/dt_arithmetic.h
	typedef enum {
		DT_EXCESS, // tnt excess
		DT_LIMIT,  // tnt none
		DT_SNAP    // tnt last
	} dt_adjust_t;

	enum interval_fields {
		FIELD_YEAR = 0,
		FIELD_MONTH,
		FIELD_WEEK,
		FIELD_DAY,
		FIELD_HOUR,
		FIELD_MINUTE,
		FIELD_SECOND,
		FIELD_NANOSECOND,
		FIELD_ADJUST,
	};

	// https://github.com/tarantool/tarantool/blob/master/src/lib/core/datetime.h
	int64_t parts[] = {0, 0, 0, 0, 0, 0, 0, 0};
	dt_adjust_t adjust = DT_LIMIT; // tnt default

	int total_length = 0;
	uint8_t ext_payload_count = mp_load_u8(&val);
	while (ext_payload_count-- > 0) {
		uint64_t key = mp_decode_uint(&val);
		int64_t value;
		enum mp_type type = mp_typeof(*val);
		if ((type != MP_UINT && type != MP_INT) || mp_read_int64(&val, &value) != 0)
			return -1;
		if (key < FIELD_ADJUST)
			parts[key] = value;
		else if (key == FIELD_ADJUST) {
			if (value > (int64_t)DT_SNAP || value < 0)
				return -1;
			adjust = (dt_adjust_t)value;
		}
	}

	/*if (parts[0] == 0 && parts[1] == 0) {
		// compose unix time (difference in seconds)
		int64_t epoch = parts[2] * 7 * 24 * 3600 + parts[3] * 24 * 3600 + parts[4] * 3600 + parts[5] * 60 + parts[6];
		PRINT_UNIX_TIME(epoch, (int32_t)parts[7]);
	} else*/ {
		// compose a map
		char *labels[] = {"year", "month", "week", "day", "hour", "min", "sec", "nsec"};
		PRINT(snprintf, "{")
		for (size_t i = 0; i < FIELD_ADJUST; ++i) {
			if (parts[i])
				PRINT(snprintf, "%s\"%s\": %ld", (total_length > 1 ? ", " : ""), labels[i], parts[i])
		}
		if (adjust != DT_LIMIT && total_length > 1) {
			char *adjust_labels[] = {"excess", "none", "last"};
			PRINT(snprintf, ", \"adjust\": \"%s\"", adjust_labels[adjust])
		}
		PRINT(snprintf, "}")
	}

	return total_length;
}

int
mp_snprint_ext_tnt(char *buf, int size, const char **data, int depth)
{
	int8_t type;
	uint32_t len = mp_decode_extl(data, &type);
	const char *ext = *data;
	*data += len;
	int res_length = 0;
	switch(type) {
	case MP_DECIMAL:
		res_length = print_decimal(&buf, size, ext, len);
		break;
	case MP_UUID:
		res_length = print_uuid(&buf, size, ext, len);
		break;
	case MP_ERROR:
		res_length = print_error_stack(&buf, size, ext, len);
		break;
	case MP_DATETIME:
		res_length = print_datetime(&buf, size, ext, len);
		break;
	case MP_INTERVAL:
		res_length = print_interval(&buf, size, ext, len);
		break;
	default:
		return mp_snprint_ext_default(buf, size, &ext, depth);
	}

	if (buf && res_length < size && size > 0)
		*buf = '\0';
	return res_length;
}

int
mp_fprint_ext_tnt(FILE *file, const char **data, int depth)
{
	int8_t type;
	int res_length = 0;
	uint32_t len = mp_decode_extl(data, &type);
	const char *ext = *data;
	*data += len;
	int (*buf_printer)(char **, size_t, const char *, uint32_t);
	size_t first_try_buf_size = 0;
	switch(type) {
	case MP_DECIMAL:
		buf_printer = print_decimal;
		first_try_buf_size = 128;
		break;
	case MP_UUID:
		buf_printer = print_uuid;
		first_try_buf_size = 64; // 39 required
		break;
	case MP_ERROR:
		buf_printer = print_error_stack;
		first_try_buf_size = 1024;
		break;
	case MP_DATETIME:
		buf_printer = print_datetime;
		first_try_buf_size = 32;
		break;
	case MP_INTERVAL:
		buf_printer = print_interval;
		first_try_buf_size = 128;
		break;
	default:
		return mp_fprint_ext_default(file, &ext, depth);
	}

	char *static_buf = alloca(first_try_buf_size);
	char *pos = static_buf;
	res_length = buf_printer(&pos,
							 first_try_buf_size,
							 ext,
							 len);
	if (res_length <= 0)
		return res_length;

	if ((size_t)res_length > first_try_buf_size) {
		char *dynamic_buf = malloc(res_length + 1);
		pos = dynamic_buf;
		buf_printer(&pos,
					res_length + 1,
					ext,
					len);
		fprintf(file, "%.*s", res_length, dynamic_buf);
		free(dynamic_buf);
	} else {
		fprintf(file, "%.*s", res_length, static_buf);
	}

	return res_length;
}
