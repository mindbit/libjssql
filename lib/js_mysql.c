#include <mysql/mysql.h>
#include "js_mysql.h"

static JSClass MysqlConnection_class = {
	"MysqlConnection", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, NULL,
};

static JSBool MysqlDriver_acceptsURL(JSContext *cx, unsigned argc, jsval *vp)
{
	JSString *url_str = JS_ValueToString(cx, JS_ARGV(cx, vp)[0]);
	// FIXME check return value
	// FIXME root url_str (protect from GC) -> https://developer.mozilla.org/en-US/docs/SpiderMonkey/JSAPI_Reference/JS_ValueToString

	char *url = JS_EncodeString(cx, url_str);
	printf("mysql check: '%s'\n", url);
	if (strncmp(url, "mysql://", 8))
		JS_SET_RVAL(cx, vp, JSVAL_FALSE);
	else
		JS_SET_RVAL(cx, vp, JSVAL_TRUE);
	JS_free(cx, url);

	return JS_TRUE;
}

static JSBool MysqlDriver_connect(JSContext *cx, unsigned argc, jsval *vp)
{
	JSBool ret = JS_FALSE;
	jsval rval = JSVAL_NULL;
	if (!argc)
		goto out;

	JSString *url_str = JS_ValueToString(cx, JS_ARGV(cx, vp)[0]);
	// FIXME check return value
	// FIXME root url_str (protect from GC) -> https://developer.mozilla.org/en-US/docs/SpiderMonkey/JSAPI_Reference/JS_ValueToString

	/* If we can't parse the URL, just return NULL (but no error) to
	 * allow the DriverManager code to try the next driver */
	ret = JS_TRUE;

	char *url = JS_EncodeString(cx, url_str);
	if (strncmp(url, "mysql://", 8))
		goto out_clean;

	char *host = url + 8;
	char *db = host + strcspn(host, "/:");
	unsigned int port = 3306;
	char *port_str;

	switch (*db) {
	case ':':
		port_str = ++db;
		db += strcspn(db, "/");
		if (*db != '/')
			goto out_clean;
		*(db++) = '\0';
		port = atoi(port_str);
		break;
	case '/':
		*(db++) = '\0';
		break;
	default:
		goto out_clean;
	}

	char *user = NULL, *passwd = NULL;
	if (argc > 1 && !JSVAL_IS_PRIMITIVE(JS_ARGV(cx, vp)[1]) && !JSVAL_IS_NULL(JS_ARGV(cx, vp)[1])) {
		JSObject *info = JSVAL_TO_OBJECT(JS_ARGV(cx, vp)[1]);
		jsval user_jsv, passwd_jsv;

		if (JS_GetProperty(cx, info, "user", &user_jsv)) {
			JSString *user_str = JS_ValueToString(cx, user_jsv);
			user = JS_EncodeString(cx, user_str);
		}

		if (JS_GetProperty(cx, info, "password", &user_jsv)) {
			JSString *passwd_str = JS_ValueToString(cx, passwd_jsv);
			passwd = JS_EncodeString(cx, passwd_str);
		}
	}

	MYSQL *mysql = mysql_init(NULL);
	MYSQL *conn = mysql_real_connect(mysql, host, user, passwd, db, port, NULL, 0);

	if (user != NULL)
		JS_free(cx, user);
	if (passwd != NULL)
		JS_free(cx, passwd);

	if (!conn) {
		ret = JS_FALSE;
		JS_ReportError(cx, mysql_error(mysql));
		mysql_close(mysql); //free(mysql);
		goto out_clean;
	}

	/* Connection is successful. Create a new connection object and
	 * link the mysql object to it. */
	JSObject *robj =  JS_NewObject(cx, NULL, NULL, NULL);
	JS_SetPrivate(robj, mysql);
	rval = OBJECT_TO_JSVAL(robj);

out_clean:
	JS_free(cx, url);
out:
	JS_SET_RVAL(cx, vp, rval);
	return ret;
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
