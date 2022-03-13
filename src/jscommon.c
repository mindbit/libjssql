/* SPDX-License-Identifier: MIT */

#include <jsmisc.h>
#include "jscommon.h"

#if 0
/**
 * getConnection - search for a connection property of an object
 * and return the reference of the property
 * @cx: JavaScript context
 * @vp: the return value will be the connection property
 *
 * Returns JS_TRUE on success and JS_FALSE on failure
 */
JSBool getConnection(JSContext *cx, jsval *vp)
{
	jsval rval = JSVAL_NULL;
	JSBool ret = JS_FALSE;
	if (JS_LookupProperty(cx, JS_THIS_OBJECT(cx, vp), "connection", &rval))
		ret = JS_TRUE;

	JS_SET_RVAL(cx, vp, rval);
	return ret;
}

/**
 * createStatement - create a Statement object
 * @cx: JavaScript context
 * @vp: the return value will be the created object
 * @class: the class of the object
 * @functions: the functions assigned to the object
 *
 * Returns JS_TRUE on success and JS_FALSE on failure
 */
JSBool createStatement(JSContext *cx, jsval *vp, JSClass *class, JSFunctionSpec *functions)
{
	JSBool ret = JS_TRUE;
	jsval rval = JSVAL_NULL;

	JSObject *obj = JS_NewObject(cx, class, NULL, NULL);
	if (obj == NULL) {
		ret = JS_FALSE;
		JS_ReportError(cx, "Failed to create a new object\n");
		JS_Log(JS_LOG_ERR, "Failed to create a new object\n");
		goto out;
	}

	JS_DefineProperty(cx, obj, "connection",
			JS_THIS(cx, vp), NULL, NULL,
			JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);

	if (JS_DefineFunctions(cx, obj, functions) == JS_FALSE) {
		ret = JS_FALSE;
		JS_ReportError(cx, "Failed to define functions in createStatement\n");
		JS_Log(JS_LOG_ERR, "Failed to define functions in createStatement\n");
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

#endif
