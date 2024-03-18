/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Nokia
 */

#include <odp/api/hints.h>
#include <odp_string_internal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

ODP_PRINTF_FORMAT(3, 4)
int _odp_snprint(char *str, size_t size, const char *format, ...)
{
	va_list args;
	int len;

	/* No space to print new characters */
	if (size < 1)
		return 0;

	if (size < 2) {
		str[0] = 0;
		return 0;
	}

	va_start(args, format);
	len = vsnprintf(str, size, format, args);
	va_end(args);

	/* Error. Ensure that string has the end mark */
	if (len < 0) {
		str[0] = 0;
		return 0;
	}

	/* Print would have been longer. Return the number of characters printed. */
	if (len >= (int)size)
		return (int)size - 1;

	return len;
}

char *_odp_strcpy(char *restrict dst, const char *restrict src, size_t sz)
{
	if (!sz)
		return dst;

	strncpy(dst, src, sz - 1);
	dst[sz - 1] = 0;
	return dst;
}
