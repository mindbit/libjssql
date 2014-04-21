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

 /**
  * JSString_to_CString - convert JSString value to an array of chars
  * @cx: JavaScript context
  * @v: JSString value
  *
  * Returns NULL on failure and a pointer to the converted value on success
  */

#include "js_utils.h"

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

	JS_RemoveStringRoot(cx, &value);

	return value_str;

out_clean:
	JS_RemoveStringRoot(cx, &value);
out:
	return NULL;
}
