#include <mysql/mysql.h>
#include <assert.h>
#include <iconv.h>
#include "js_mysql.h"

struct prepared_statement {
	MYSQL_STMT *stmt;

	// parameters
	MYSQL_BIND *p_bind;
	unsigned int p_len;

	// results
	MYSQL_BIND *r_bind;
	unsigned int r_len;
	unsigned long *r_bind_len;
	my_bool *r_is_null;
};

/* {{{ MysqlStatement */

static JSBool MysqlStatement_execute(JSContext *cx, unsigned argc, jsval *vp)
{
	return JS_FALSE;
}

static JSBool MysqlStatement_executeQuery(JSContext *cx, unsigned argc, jsval *vp)
{
	return JS_FALSE;
}

static JSBool MysqlStatement_executeUpdate(JSContext *cx, unsigned argc, jsval *vp)
{
	return JS_FALSE;
}

static JSBool MysqlStatement_getConnection(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval rval = JSVAL_NULL;
	JSBool ret = JS_FALSE;
	if (JS_LookupProperty(cx, JS_THIS_OBJECT(cx, vp), "connection", &rval))
		ret = JS_TRUE;

	JS_SET_RVAL(cx, vp, rval);
	return ret;
}

static JSFunctionSpec MysqlStatement_functions[] = {
	JS_FS("execute", MysqlStatement_execute, 1, 0),
	JS_FS("executeQuery", MysqlStatement_executeQuery, 1, 0),
	JS_FS("executeUpdate", MysqlStatement_executeUpdate, 1, 0),
	JS_FS("getConnection", MysqlStatement_getConnection, 1, 0),
	JS_FS_END
};

/* }}} MysqlStatement */

/* {{{ MysqlPreparedResultSet */

static JSClass MysqlPreparedResultSet_class = {
	"MysqlPreparedResultSet", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, NULL,
};

static inline JSBool MysqlPreparedResultSet_get(JSContext *cx, unsigned argc, jsval *vp, struct prepared_statement **pstmt, uint32_t *i, JSBool *ret)
{
	jsval this = JS_THIS(cx, vp);
	*pstmt = (struct prepared_statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));
	*ret = JS_FALSE;

	if (argc < 1) {
		JS_SET_RVAL(cx, vp, JSVAL_NULL);
		return JS_FALSE;
	}
	if (!JS_ValueToECMAUint32(cx, JS_ARGV(cx, vp)[0], i)) {
		JS_SET_RVAL(cx, vp, JSVAL_NULL);
		return JS_FALSE;
	}
	if (*i < 1 || *i > (*pstmt)->r_len) {
		JS_SET_RVAL(cx, vp, JSVAL_NULL);
		return JS_FALSE;
	}
	(*i)--;
	if ((*pstmt)->r_is_null[*i]) {
		JS_SET_RVAL(cx, vp, JSVAL_NULL);
		*ret = JS_TRUE;
		return JS_FALSE;
	}

	return JS_TRUE;
}

static JSBool MysqlPreparedResultSet_getNumber(JSContext *cx, unsigned argc, jsval *vp)
{
	JSBool ret;
	struct prepared_statement *pstmt;
	uint32_t i;

	if (!MysqlPreparedResultSet_get(cx, argc, vp, &pstmt, &i, &ret))
		return ret;

	MYSQL_BIND bind;
	double val;

	memset(&bind, 0, sizeof(MYSQL_BIND));
	bind.buffer_type = MYSQL_TYPE_DOUBLE;
	bind.buffer = &val;
	bind.buffer_length = sizeof(double);

	if (mysql_stmt_fetch_column(pstmt->stmt, &bind, i, 0)) {
		JS_ReportError(cx, mysql_stmt_error(pstmt->stmt));
		return JS_FALSE;
	}

	JS_SET_RVAL(cx, vp, JS_NumberValue(val));
	return JS_TRUE;
}

static JSBool MysqlPreparedResultSet_getString(JSContext *cx, unsigned argc, jsval *vp)
{
	JSBool ret;
	struct prepared_statement *pstmt;
	uint32_t i;

	if (!MysqlPreparedResultSet_get(cx, argc, vp, &pstmt, &i, &ret))
		return ret;

	if (!pstmt->r_bind_len[i]) {
		JS_SET_RVAL(cx, vp, JS_GetEmptyStringValue(cx));
		return JS_TRUE;
	}

	MYSQL_BIND bind;

	char *cbuf = malloc(pstmt->r_bind_len[i]);
	assert(cbuf);

	memset(&bind, 0, sizeof(MYSQL_BIND));
	bind.buffer_type = MYSQL_TYPE_STRING;
	bind.buffer = cbuf;
	bind.buffer_length = pstmt->r_bind_len[i];

	if (mysql_stmt_fetch_column(pstmt->stmt, &bind, i, 0)) {
		JS_ReportError(cx, mysql_stmt_error(pstmt->stmt));
		return JS_FALSE;
	}

	size_t jslen;

	/* First determine the necessary size for the js buffer.
	 * This is well documented in jsapi.h, just before the
	 * prototype for JS_EncodeCharacters().
	 */
	JS_DecodeBytes(cx, cbuf, pstmt->r_bind_len[i], NULL, &jslen); // FIXME check return value
	jschar *jsbuf = JS_malloc(cx, (jslen + 1) * sizeof(jschar));
	assert(jsbuf);

	JS_DecodeBytes(cx, cbuf, pstmt->r_bind_len[i], jsbuf, &jslen); // FIXME check return value
	free(cbuf);

	/* Add a null terminator, or we get an assertion failure if mozjs
	 * is compiled with debugging */
	jsbuf[jslen] = 0;

	JSString *val = JS_NewUCString(cx, jsbuf, jslen);
	if (!val)
		return JS_FALSE;

	JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(val));
	return JS_TRUE;
}

static JSBool MysqlPreparedResultSet_next(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval this = JS_THIS(cx, vp);
	struct prepared_statement *pstmt = (struct prepared_statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));

	switch (mysql_stmt_fetch(pstmt->stmt)) {
	case 0:
	case MYSQL_DATA_TRUNCATED:
		JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(JS_TRUE));
		return JS_TRUE;
	case MYSQL_NO_DATA:
		JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(JS_FALSE));
		return JS_TRUE;
	default:
		JS_SET_RVAL(cx, vp, JSVAL_NULL);
		JS_ReportError(cx, mysql_stmt_error(pstmt->stmt));
	}

	return JS_FALSE;
}

static JSFunctionSpec MysqlPreparedResultSet_functions[] = {
	JS_FS("getNumber", MysqlPreparedResultSet_getNumber, 1, 0),
	JS_FS("getString", MysqlPreparedResultSet_getString, 1, 0),
	JS_FS("next", MysqlPreparedResultSet_next, 0, 0),
	JS_FS_END
};

/* }}} MysqlPreparedResultSet */

/* {{{ MysqlPreparedStatement */

static JSClass MysqlPreparedStatement_class = {
	"MysqlConnection", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, NULL,
};

static JSBool prepared_statement_execute(JSContext *cx, struct prepared_statement *pstmt)
{
	unsigned int i;

	if (pstmt->p_len && mysql_stmt_bind_param(pstmt->stmt, pstmt->p_bind)) {
		JS_ReportError(cx, mysql_stmt_error(pstmt->stmt));
		return JS_FALSE;
	}

	if (mysql_stmt_execute(pstmt->stmt)) {
		JS_ReportError(cx, mysql_stmt_error(pstmt->stmt));
		return JS_FALSE;
	}

	for (i = 0; i < pstmt->p_len; i++)
		if (pstmt->p_bind[i].buffer)
			free(pstmt->p_bind[i].buffer);

	if (pstmt->r_len && mysql_stmt_bind_result(pstmt->stmt, pstmt->r_bind)) {
		JS_ReportError(cx, mysql_stmt_error(pstmt->stmt));
		return JS_FALSE;
	}

	return JS_TRUE;
}

static JSBool prepared_statement_get_result_set(JSContext *cx, struct prepared_statement *pstmt, jsval *rval)
{
	JSObject *robj = JS_NewObject(cx, &MysqlPreparedResultSet_class, NULL, NULL);
	JS_SetPrivate(robj, pstmt);
	JS_DefineFunctions(cx, robj, MysqlPreparedResultSet_functions);

	*rval = OBJECT_TO_JSVAL(robj);
	return JS_TRUE;
}

static JSBool MysqlPreparedStatement_execute(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval this = JS_THIS(cx, vp);
	struct prepared_statement *pstmt = (struct prepared_statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));

	if (!prepared_statement_execute(cx, pstmt))
		return JS_FALSE;

	JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(pstmt->r_len > 0));
	return JS_TRUE;
}

static JSBool MysqlPreparedStatement_executeQuery(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval this = JS_THIS(cx, vp);
	struct prepared_statement *pstmt = (struct prepared_statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));

	if (!prepared_statement_execute(cx, pstmt))
		return JS_FALSE;

	prepared_statement_get_result_set(cx, pstmt, &JS_RVAL(cx, vp));
	return JS_TRUE;
}

static JSBool MysqlPreparedStatement_executeUpdate(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval this = JS_THIS(cx, vp);
	struct prepared_statement *pstmt = (struct prepared_statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));

	if (!prepared_statement_execute(cx, pstmt))
		return JS_FALSE;

	my_ulonglong rows = mysql_stmt_affected_rows(pstmt->stmt);
	JS_SET_RVAL(cx, vp, JS_NumberValue(rows));
	return JS_TRUE;
}

static JSBool MysqlPreparedStatement_getResultSet(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval this = JS_THIS(cx, vp);
	struct prepared_statement *pstmt = (struct prepared_statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));

	if (!pstmt->r_len) {
		JS_SET_RVAL(cx, vp, JSVAL_NULL);
		return JS_TRUE;
	}

	prepared_statement_get_result_set(cx, pstmt, &JS_RVAL(cx, vp));
	return JS_TRUE;
}

static JSBool MysqlPreparedStatement_getUpdateCount(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval this = JS_THIS(cx, vp);
	struct prepared_statement *pstmt = (struct prepared_statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));

	if (pstmt->r_len) {
		my_ulonglong rows = mysql_stmt_affected_rows(pstmt->stmt);
		JS_SET_RVAL(cx, vp, JS_NumberValue(rows));
	} else
		JS_SET_RVAL(cx, vp, JS_NumberValue(-1));

	return JS_TRUE;
}

static JSBool MysqlPreparedStatement_setString(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval this = JS_THIS(cx, vp);
	struct prepared_statement *pstmt = (struct prepared_statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));
	uint32_t p;

	if (argc < 1)
		return JS_FALSE;
	if (!JS_ValueToECMAUint32(cx, JS_ARGV(cx, vp)[0], &p))
		return JS_FALSE;
	if (p < 1 || p > pstmt->p_len)
		return JS_FALSE;
	if (pstmt->p_bind[p - 1].buffer) {
		free(pstmt->p_bind[p - 1].buffer);
		// prevent second free in prepared_statement_execute()
		pstmt->p_bind[p - 1].buffer = NULL;
	}
	if (argc < 2 || JSVAL_IS_NULL(JS_ARGV(cx, vp)[1])) {
		JS_SET_RVAL(cx, vp, JSVAL_NULL);
		pstmt->p_bind[p - 1].buffer_type = MYSQL_TYPE_NULL;
		return JS_TRUE;
	}

	JSString *str = JS_ValueToString(cx, JS_ARGV(cx, vp)[1]);

	size_t jslen, clen;
	const jschar *jsbuf = JS_GetStringCharsAndLength(cx, str, &jslen);

	/* First determine the necessary size for the js buffer.
	 * This is well documented in jsapi.h, just before the
	 * prototype for JS_EncodeCharacters().
	 */
	JS_EncodeCharacters(cx, jsbuf, jslen, NULL, &clen); // FIXME check return value
	char *cbuf = malloc(clen);
	assert(cbuf);

	JS_EncodeCharacters(cx, jsbuf, jslen, cbuf, &clen); // FIXME check return value
	pstmt->p_bind[p - 1].buffer_type = MYSQL_TYPE_STRING;
	pstmt->p_bind[p - 1].buffer = cbuf;
	pstmt->p_bind[p - 1].buffer_length = clen;

	JS_SET_RVAL(cx, vp, JSVAL_NULL);
	return JS_TRUE;
}

static JSFunctionSpec MysqlPreparedStatement_functions[] = {
	JS_FS("execute", MysqlPreparedStatement_execute, 0, 0),
	JS_FS("executeQuery", MysqlPreparedStatement_executeQuery, 0, 0),
	JS_FS("executeUpdate", MysqlPreparedStatement_executeUpdate, 0, 0),
	JS_FS("getConnection", MysqlStatement_getConnection, 1, 0),
	JS_FS("getResultSet", MysqlPreparedStatement_getResultSet, 0, 0),
	JS_FS("getUpdateCount", MysqlPreparedStatement_getUpdateCount, 0, 0),
	JS_FS("setString", MysqlPreparedStatement_setString, 2, 0),
	JS_FS_END
};

/* }}} MysqlPreparedStatement */

/* {{{ MysqlConnection */

static JSClass MysqlConnection_class = {
	"MysqlConnection", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, NULL,
};

static JSBool MysqlConnection_createStatement(JSContext *cx, unsigned argc, jsval *vp)
{
	JSObject *obj = JS_NewObject(cx, NULL, NULL, NULL);

	JS_DefineProperty(cx, obj, "connection", JS_THIS(cx, vp), NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineFunctions(cx, obj, MysqlStatement_functions); // FIXME check return value

	JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(obj));
	return JS_TRUE;
}

static JSBool MysqlConnection_nativeSQL(JSContext *cx, unsigned argc, jsval *vp)
{
	if (argc < 1) {
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

static JSBool MysqlConnection_prepareStatement(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval this = JS_THIS(cx, vp);
	jsval nativeSQL_argv[] = {JS_ARGV(cx, vp)[0]};
	jsval nativeSQL_jsv;

	if (!JS_CallFunctionName(cx, JSVAL_TO_OBJECT(this), "nativeSQL", 1, nativeSQL_argv, &nativeSQL_jsv))
		return JS_FALSE;

	JSString *nativeSQL_str = JS_ValueToString(cx, nativeSQL_jsv);
	char *nativeSQL = JS_EncodeString(cx, nativeSQL_str);

	MYSQL *mysql = (MYSQL *)JS_GetPrivate(JSVAL_TO_OBJECT(this));
	struct prepared_statement *pstmt = malloc(sizeof(struct prepared_statement));
	assert(pstmt); // FIXME return error
	memset(pstmt, 0, sizeof(struct prepared_statement));

	pstmt->stmt = mysql_stmt_init(mysql);
	if (!pstmt->stmt)
		return JS_FALSE; // FIXME free(pstmt)

	if (mysql_stmt_prepare(pstmt->stmt, nativeSQL, strlen(nativeSQL))) {
		JS_ReportError(cx, mysql_stmt_error(pstmt->stmt));
		return JS_FALSE; // FIXME free(pstmt)
	}

	pstmt->p_len = mysql_stmt_param_count(pstmt->stmt);
	if (pstmt->p_len) {
		pstmt->p_bind = malloc(pstmt->p_len * sizeof(MYSQL_BIND));
		assert(pstmt->p_bind);
		memset(pstmt->p_bind, 0, pstmt->p_len * sizeof(MYSQL_BIND));
	}

	/* Invoke mysql_stmt_fetch() with a zero-length buffer for all
	 * colums, and pointers in which the real length can be stored.
	 * This allows us to use mysql_stmt_fetch_column() with the
	 * real length and to not force column types before calling
	 * mysql_stmt_fetch(). This maps well to the JDBC API, where
	 * individual colums are retrieved from the ResultSet after
	 * executing the query.
	 *
	 * Using zero-length buffers is documented in the
	 * mysql_stmt_fetch() manual page:
	 * http://dev.mysql.com/doc/refman/5.1/en/mysql-stmt-fetch.html
	 */
	pstmt->r_len = mysql_stmt_field_count(pstmt->stmt);
	if (pstmt->r_len) {
		pstmt->r_bind = malloc(pstmt->r_len * sizeof(MYSQL_BIND));
		assert(pstmt->r_bind);
		memset(pstmt->r_bind, 0, pstmt->r_len * sizeof(MYSQL_BIND));
		pstmt->r_bind_len = malloc(pstmt->r_len * sizeof(*(pstmt->r_bind_len)));
		assert(pstmt->r_bind_len);
		pstmt->r_is_null = malloc(pstmt->r_len * sizeof(my_bool));
		assert(pstmt->r_is_null);
	}

	unsigned int i;
	for (i = 0; i < pstmt->r_len; i++) {
		pstmt->r_bind[i].length = &pstmt->r_bind_len[i];
		pstmt->r_bind[i].is_null = &pstmt->r_is_null[i];
	}

	JSObject *robj = JS_NewObject(cx, &MysqlPreparedStatement_class, NULL, NULL);
	JS_SetPrivate(robj, pstmt);

	JS_DefineProperty(cx, robj, "connection", this, NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineFunctions(cx, robj, MysqlPreparedStatement_functions); // FIXME check return value

	JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(robj));
	return JS_TRUE;
}

static JSFunctionSpec MysqlConnection_functions[] = {
	JS_FS("createStatement", MysqlConnection_createStatement, 0, 0),
	JS_FS("nativeSQL", MysqlConnection_nativeSQL, 1, 0),
	JS_FS("prepareStatement", MysqlConnection_prepareStatement, 1, 0),
	JS_FS_END
};

/* }}} MysqlConnection */

/* {{{ MysqlDriver */

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

		if (JS_GetProperty(cx, info, "user", &user_jsv) && !JSVAL_IS_NULL(user_jsv)) {
			JSString *user_str = JS_ValueToString(cx, user_jsv);
			user = JS_EncodeString(cx, user_str);
		}

		if (JS_GetProperty(cx, info, "password", &passwd_jsv) && !JSVAL_IS_NULL(passwd_jsv)) {
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

	mysql_set_character_set(mysql, "utf8"); // FIXME check return value

	/* Connection is successful. Create a new connection object and
	 * link the mysql object to it. */
	JSObject *robj =  JS_NewObject(cx, &MysqlConnection_class, NULL, NULL);
	JS_SetPrivate(robj, mysql);
	JS_DefineFunctions(cx, robj, MysqlConnection_functions); // FIXME check return value
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

/* }}} MysqlDriver */

JSBool JS_MysqlConstructAndRegister(JSContext *cx, JSObject *global)
{
	JSObject *obj = JS_NewObject(cx, NULL, NULL, NULL);

	if (!JS_DefineFunctions(cx, obj, MysqlDriver_functions))
		return JS_FALSE;

	return JS_SqlRegisterDriver(cx, global, obj);
}

// vim: foldmethod=marker
