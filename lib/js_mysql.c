#include "js_mysql.h"

static JSBool MysqlDriver_acceptsURL(JSContext *cx, unsigned argc, jsval *vp)
{
	JS_SET_RVAL(cx, vp, JSVAL_NULL);

	JSString *url_str = JS_ValueToString(cx, JS_ARGV(cx, vp)[0]);
	// FIXME check return value
	// FIXME root url_str (protect from GC) -> https://developer.mozilla.org/en-US/docs/SpiderMonkey/JSAPI_Reference/JS_ValueToString
	
	char *url = JS_EncodeString(cx, url_str);
	printf("mysql check: '%s'\n", url);
	JS_free(cx, url);

	return JS_TRUE;
}

static JSBool MysqlDriver_connect(JSContext *cx, unsigned argc, jsval *vp)
{
	JS_SET_RVAL(cx, vp, JSVAL_NULL);
	return JS_TRUE;
}

#if 0
static JSClass MysqlDriver_class = {
	"MysqlDriver", 0,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, NULL,
};
#endif

static JSFunctionSpec MysqlDriver_functions[] = {
	JS_FS("acceptsURL", MysqlDriver_acceptsURL, 1, 0),
	JS_FS("connect", MysqlDriver_connect, 2, 0),
	JS_FS_END
};

JSBool JS_MysqlConstructAndRegister(JSContext *cx, JSObject *global)
{
	JSObject *obj = JS_NewObject(cx, NULL, NULL, NULL);

	if (!JS_DefineFunctions(cx, obj, MysqlDriver_functions))
		return JS_FALSE;

	return JS_SqlRegisterDriver(cx, global, obj);
}
