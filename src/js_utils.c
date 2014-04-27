/* libjssql - a JDBC-like SQL API for SpiderMonkey JS
 * Copyright (C) 2013 Mindbit SRL
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of version 2.1 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "js_utils.h"

 /**
  * JSString_to_CString - convert JSString value to an array of chars
  * @cx: JavaScript context
  * @v: JSString value
  *
  * Returns NULL on failure and a pointer to the converted value on success
  */
char *JSString_to_CString(JSContext * cx, jsval v)
{
	JSString *value = JS_ValueToString(cx, v);

	if (value == NULL) {
		dlog(LOG_ALERT, "Failed to convert the value to JSString\n");
		goto out;
	}

	if (JS_AddStringRoot(cx, &value) == JS_FALSE) {
		dlog(LOG_ALERT, "Failed to root the string value\n");
		goto out;
	}

	char *value_str = JS_EncodeString(cx, value);
	if (value_str == NULL) {
		dlog(LOG_ALERT, "Failed to encode the string value\n");
		goto out_clean;
	}

	//FIXME not sure is this is safe
	JS_RemoveStringRoot(cx, &value);

	return value_str;

out_clean:
	JS_RemoveStringRoot(cx, &value);
out:
	return NULL;
}

/**
 * my_itoa - convert n to characters and returns the result
 * @n: number which will be converted
 */
 char *my_itoa(int n)
{
	int i, j, sign, digits, n_copy;
	char *result;

	digits = 0;
	
	if ((sign = n) < 0) { /* record sign */
		n = -n;          /* make n positive */
		digits++;
	}

	n_copy = n;

	/* count the digits */
	do {
		digits++;
		n_copy = n_copy / 10;
	} while(n_copy > 0);

	result = malloc((digits + 1) * sizeof(char));
	if (result == NULL)
		return NULL;

	i = 0;
	
	do {       /* generate digits in reverse order */
		result[i++] = n % 10 + '0';   /* get next digit */
	} while ((n /= 10) > 0);     /* delete it */
	
	if (sign < 0)
		result[i++] = '-';
	result[i] = '\0';

	char c;

	for (i = 0, j = digits - 1; i < j; i++, j--) {
		c = result[i];
		result[i] = result[j];
		result[j] = c;
	}

	return result;
}
