#include "js_sql.h"

static JSBool DriverManager_getDriver(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval drivers;
	JS_LookupProperty(cx, JS_THIS_OBJECT(cx, vp), "drivers", &drivers);
	// FIXME check return code; check that drivers is an array
	JSObject *obj = JSVAL_TO_OBJECT(drivers);

	jsuint len, i;
	JS_GetArrayLength(cx, obj, &len);

	for (i = 0; i < len; i++) {
		jsval driver, rval;
		jsval acceptsURL_argv[] = {JS_ARGV(cx, vp)[0]};

		JS_GetElement(cx, obj, i, &driver);
		JS_CallFunctionName(cx, JSVAL_TO_OBJECT(driver), "acceptsURL", 1, &acceptsURL_argv[0], &rval); // FIXME check return value

		JSBool acceptsURL;
		JS_ValueToBoolean(cx, rval, &acceptsURL); // FIXME check ret val
		if (acceptsURL) {
			JS_SET_RVAL(cx, vp, driver);
			return JS_TRUE;
		}
	}

	JS_ReportError(cx, "No suitable driver");
	JS_SET_RVAL(cx, vp, JSVAL_NULL);
	return JS_FALSE;
}

static JSBool DriverManager_getConnection(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval drivers;
	JS_LookupProperty(cx, JS_THIS_OBJECT(cx, vp), "drivers", &drivers);
	// FIXME check return code; check that drivers is an array
	JSObject *obj = JSVAL_TO_OBJECT(drivers);

	jsuint len, i;
	JS_GetArrayLength(cx, obj, &len);


	jsval connect_argv[2] = {JS_ARGV(cx, vp)[0]};
	if (argc == 2) {
		/* Caller passed "info" object, so we forward it as-is */
		connect_argv[1] = JS_ARGV(cx, vp)[1];
	} else {
		JSObject *info = JS_NewObject(cx, NULL, NULL, NULL); // FIXME root it to avoid GC
		if (argc > 1)
			JS_DefineProperty(cx, info, "user", JS_ARGV(cx, vp)[1], NULL, NULL, JSPROP_ENUMERATE);
		if (argc > 2)
			JS_DefineProperty(cx, info, "password", JS_ARGV(cx, vp)[2], NULL, NULL, JSPROP_ENUMERATE);

		connect_argv[1] = OBJECT_TO_JSVAL(info);
	};

	jsval reason = JSVAL_NULL;
	for (i = 0; i < len; i++) {
		jsval driver, rval;
		JS_GetElement(cx, obj, i, &driver);

		if (!JS_CallFunctionName(cx, JSVAL_TO_OBJECT(driver), "connect", 2, &connect_argv[0], &rval)) {
			if (JSVAL_IS_NULL(reason))
				JS_GetPendingException(cx, &reason);
			continue;
		}
		if (JSVAL_IS_NULL(rval))
			continue;
		JS_SET_RVAL(cx, vp, rval);
		return JS_TRUE;
	}

	if (JSVAL_IS_NULL(reason)) {
		JSString *url_str = JS_ValueToString(cx, JS_ARGV(cx, vp)[0]);
		// FIXME check return value
		// FIXME root url_str (protect from GC) -> https://developer.mozilla.org/en-US/docs/SpiderMonkey/JSAPI_Reference/JS_ValueToString
		char *url = JS_EncodeString(cx, url_str);
		JS_ReportError(cx, "No suitable driver found for %s", url);
		JS_free(cx, url);
	} else
		JS_SetPendingException(cx, reason);

	JS_SET_RVAL(cx, vp, JSVAL_NULL);
	return JS_FALSE;
}

static JSBool DriverManager_registerDriver(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval drivers;
	JS_LookupProperty(cx, JS_THIS_OBJECT(cx, vp), "drivers", &drivers);
	// FIXME check return code; check that drivers is an array
	JSObject *obj = JSVAL_TO_OBJECT(drivers);

	jsuint len;
	JS_GetArrayLength(cx, obj, &len);
	JS_SetElement(cx, obj, len, JS_ARGV(cx, vp));

	JS_SET_RVAL(cx, vp, JSVAL_NULL);
	return JS_TRUE;
}

static JSClass DriverManager_class = {
	"DriverManager", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, NULL,
};

static JSFunctionSpec DriverManager_functions[] = {
	JS_FS("getConnection", DriverManager_getConnection, 3, 0),
	JS_FS("getDriver", DriverManager_getDriver, 1, 0),
	JS_FS("registerDriver", DriverManager_registerDriver, 1, 0),
	JS_FS_END
};

JSBool JS_SqlInit(JSContext *cx, JSObject *global)
{
	JSObject *obj;
	obj = JS_DefineObject(cx, global, "DriverManager", &DriverManager_class, NULL, JSPROP_ENUMERATE);

	if (!JS_DefineFunctions(cx, obj, DriverManager_functions))
		return JS_FALSE;

	JSObject *drivers = JS_NewArrayObject(cx, 0, NULL);
	if (drivers == NULL)
		return JS_FALSE;

	JS_DefineProperty(cx, obj, "drivers", OBJECT_TO_JSVAL(drivers), NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);

	return JS_TRUE;
}

JSBool JS_SqlRegisterDriver(JSContext *cx, JSObject *global, JSObject *driver)
{
	jsval DriverManager, rval;
	jsval argv[] = {
		OBJECT_TO_JSVAL(driver)
	};
	JS_GetProperty(cx, global, "DriverManager", &DriverManager); // FIXME check return value
	// FIXME check that DriverManager is an object
	JS_CallFunctionName(cx, JSVAL_TO_OBJECT(DriverManager), "registerDriver", 1, &argv[0], &rval); // FIXME check return value

	return JS_TRUE;
}
