#include "js_sql.h"

static JSBool DriverManager_getConnection(JSContext *cx, unsigned argc, jsval *vp)
{
	/* Equivalent of "return null;" from JavaScript */
	JS_SET_RVAL(cx, vp, JSVAL_NULL);

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
		printf("check driver %d!\n", i);
	}

	return JS_TRUE;
}

static JSBool DriverManager_registerDriver(JSContext *cx, unsigned argc, jsval *vp)
{
	JS_SET_RVAL(cx, vp, JSVAL_NULL);

	jsval drivers;
	JS_LookupProperty(cx, JS_THIS_OBJECT(cx, vp), "drivers", &drivers);
	// FIXME check return code; check that drivers is an array
	JSObject *obj = JSVAL_TO_OBJECT(drivers);

	jsuint len;
	JS_GetArrayLength(cx, obj, &len);

	JS_SetElement(cx, obj, len, JS_ARGV(cx, vp));

	JS_GetArrayLength(cx, obj, &len);
	printf("Called my function register %d !\n", len);
	return JS_TRUE;
}

static JSClass DriverManager_class = {
	"DriverManager", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, NULL,
};

static JSFunctionSpec DriverManager_functions[] = {
	JS_FS("getConnection", DriverManager_getConnection, 3, 0),
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
