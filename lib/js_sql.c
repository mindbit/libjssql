#include "js_sql.h"

static JSBool DriverManager_getConnection(JSContext *cx, unsigned argc, jsval *vp)
{
	/* Equivalent of "return null;" from JavaScript */
	JS_SET_RVAL(cx, vp, JSVAL_NULL);

	printf("Called my function %d!\n", argc);
	return JS_TRUE;
}

static JSClass DriverManager_class = {
	"DriverManager", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSFunctionSpec DriverManager_functions[] = {
	JS_FS("getConnection", DriverManager_getConnection, 3, 0),
	JS_FS_END
};

JSBool JS_SqlInit(JSContext *cx, JSObject *global)
{
	JSObject *obj;
	obj = JS_DefineObject(cx, global, "DriverManager", &DriverManager_class, NULL, JSPROP_ENUMERATE);

	if (!JS_DefineFunctions(cx, obj, DriverManager_functions))
		return JS_FALSE;

	return JS_TRUE;
}
