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

#include "js_common.h"

JSBool getConnection(JSContext *cx, jsval *vp)
{
	jsval rval = JSVAL_NULL;
	JSBool ret = JS_FALSE;
	if (JS_LookupProperty(cx, JS_THIS_OBJECT(cx, vp), "connection", &rval))
		ret = JS_TRUE;

	JS_SET_RVAL(cx, vp, rval);
	return ret;
}

JSBool createStatement(JSContext *cx, jsval *vp, JSClass *class, JSFunctionSpec *functions)
{
	JSBool ret = JS_TRUE;
	jsval rval = JSVAL_NULL;

	JSObject *obj = JS_NewObject(cx, class, NULL, NULL);
	if (obj == NULL) {
		ret = JS_FALSE;
		JS_ReportError(cx, "Failed to create a new object\n");
		dlog(LOG_ALERT, "Failed to create a new object\n");
		goto out;
	}

	JS_DefineProperty(cx, obj, "connection", 
					JS_THIS(cx, vp), NULL, NULL, 
					JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);

	if (JS_DefineFunctions(cx, obj, functions) == JS_FALSE) {
		ret = JS_FALSE;
		JS_ReportError(cx, "Failed to define functions in createStatement\n");
		dlog(LOG_ALERT, "Failed to define functions in createStatement\n");
		goto out;
	}

	rval = OBJECT_TO_JSVAL(obj);

out:
	JS_SET_RVAL(cx, vp, rval);
	return ret;
}


 /**
  * nativeSQL - get the first parameter and convert it to string
  * @cx: JavaScript context
  * @argc: arguments' number
  * @vp: pointer where the value will be stored
  *
  * Returns JS_TRUE on success and JS_FALSE on failure
  */
JSBool nativeSQL(JSContext *cx, unsigned argc, jsval *vp)
{
	if (argc != 1) {
		JS_SET_RVAL(cx, vp, JSVAL_NULL);
		return JS_FALSE;
	}

	JSString *str = JS_ValueToString(cx, JS_ARGV(cx, vp)[0]);
	if (str) {
		JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(str));
		return JS_TRUE;
	}

	JS_SET_RVAL(cx, vp, JSVAL_NULL);
	
	return JS_FALSE;
}
