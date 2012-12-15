/* $Id$ */

/*
 * This file is part of FreeRCT.
 * FreeRCT is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * FreeRCT is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with FreeRCT. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file string_func.cpp Safe string functions. */

#include "stdafx.h"
#include "string_func.h"

/**
 * Safe string copy (as in, the destination is always terminated).
 * @param dest Destination of the string.
 * @param src  Source of the string.
 * @param size Number of bytes (including terminating 0) from \a src.
 * @return Copied string.
 */
char *SafeStrncpy(char *dest, const char *src, int size)
{
	assert(size >= 1);

	strncpy(dest, src, size);
	dest[size - 1] = '\0';
	return dest;
}

/**
 * Duplicate a string.
 * @param src Source string.
 * @return Copy of the string in its own memory.
 */
char *StrDup(const char *src)
{
	size_t n = strlen(src);
	char *mem = (char *)malloc(n + 1);
	assert(mem != NULL);

	return SafeStrncpy(mem, src, n + 1);
}

/**
 * String copy up to an end-point of the destination.
 * @param dest Start of destination buffer.
 * @param end End of destination buffer (\c *end is never written).
 * @param src Source string.
 * @return \a dest.
 */
uint8 *StrECpy(uint8 *dest, uint8 *end, const uint8 *src)
{
	uint8 *dst = dest;
	end--;
	while (*src && dest < end) {
		*dest = *src;
		dest++;
		src++;
	}
	*dest = '\0';
	return dst;
}

/**
 * Are the two strings equal?
 * @param s1 First string to compare.
 * @param s2 Second string to compare.
 * @return strings are exactly equal.
 */
bool StrEqual(const uint8 *s1, const uint8 *s2)
{
	assert(s1 != NULL && s2 != NULL);
	while (*s1 != '\0' && *s2 != '\0' && *s1 == *s2) {
		s1++;
		s2++;
	}
	return *s1 == '\0' && *s2 == '\0';
}

/**
 * Test whether \a str ends with \a end.
 * @param str String to check.
 * @param end Expected end text.
 * @param case_sensitive Compare case sensitively.
 * @return \a str ends with the given \a end possibly after changing case.
 */
bool StrEndsWith(const char *str, const char *end, bool case_sensitive)
{
	size_t str_length = strlen(str);
	size_t end_length = strlen(end);
	if (end_length > str_length) return false;
	if (end_length == 0) return true;

	str += str_length - end_length;
	while (*str != '\0' && *end != '\0') {
		char s = *str;
		char e = *end;
		if (!case_sensitive && s >= 'A' && s <= 'Z') s += 'a' - 'A';
		if (!case_sensitive && e >= 'A' && e <= 'Z') e += 'a' - 'A';
		if (s != e) return false;
		str++; end++;
	}
	return true;
}

