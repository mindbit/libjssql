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

#define _GNU_SOURCE
#include <stdio.h>
#include <mysql.h>
#include <assert.h>
#include <jsmisc.h>

#include "jsmysql.h"
#include "jssql.h"
#include "jscommon.h"

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

struct generated_keys {
	my_ulonglong last_insert_id;
	int cursor;
};

/**
 * clear_statement - clears the prepared statement structure
 * @stmt: pointer to the structure
 */
static void clear_statement(struct prepared_statement *pstmt)
{
	unsigned int i;

	if(pstmt == NULL)
		return;

	free(pstmt->r_bind_len);
	pstmt->r_bind_len = NULL;

	free(pstmt->r_is_null);
	pstmt->r_is_null = NULL;

	/* close the statement */
	mysql_stmt_free_result(pstmt->stmt);
	mysql_stmt_close(pstmt->stmt);

	/* clear the results*/
	for (i = 0; i < pstmt->r_len; i++)
		if (pstmt->r_bind[i].buffer) {
			free(pstmt->r_bind[i].buffer);
			pstmt->r_bind[i].buffer = NULL;
		}
	free(pstmt->r_bind);
	pstmt->r_bind = NULL;

	free(pstmt);
	pstmt = NULL;

}

/**
 * MysqlGeneratedKeys_finalize - cleanup function for GeneratedKeys objects
 * @cx: JavaScript context
 * @obj: object
 */
static void MysqlGeneratedKeys_finalize(JSFreeOp *fop, JSObject *obj)
{
	struct generated_keys *priv = (struct generated_keys *)JS_GetPrivate(obj);

	if (priv) {
		free(priv);
		priv = NULL;
	}
}

/* {{{ MysqlPreparedGeneratedKeys */

static JSClass MysqlGeneratedKeys_class = {
	"MysqlGeneratedKeys", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, MysqlGeneratedKeys_finalize,
};

static JSBool MysqlGeneratedKeys_getNumber(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval this = JS_THIS(cx, vp);
	struct generated_keys *k = (struct generated_keys *)JS_GetPrivate(JSVAL_TO_OBJECT(this));

	if (k)
		JS_SET_RVAL(cx, vp, JS_NumberValue((double)k->last_insert_id));
	else
		JS_SET_RVAL(cx, vp, JSVAL_NULL);

	return JS_TRUE;
}

static JSBool MysqlGeneratedKeys_getString(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval this = JS_THIS(cx, vp);
	struct generated_keys *k = (struct generated_keys *)JS_GetPrivate(JSVAL_TO_OBJECT(this));
	char *cstr;

	if (k == NULL)
		JS_SET_RVAL(cx, vp, JSVAL_NULL);

	if (asprintf(&cstr, "%ju", (uintmax_t)k->last_insert_id) == -1)
		return JS_FALSE;
	JSString *jsstr = JS_NewStringCopyZ(cx, cstr);
	free(cstr);
	JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(jsstr));
	return JS_TRUE;
}

static JSBool MysqlGeneratedKeys_next(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval this = JS_THIS(cx, vp);
	struct generated_keys *k = (struct generated_keys *)JS_GetPrivate(JSVAL_TO_OBJECT(this));

	if (k == NULL)
		JS_SET_RVAL(cx, vp, JSVAL_FALSE);

	if (k->cursor < 1) {
		k->cursor++;
		JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(JS_TRUE));
	} else
		JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(JS_FALSE));
	return JS_TRUE;
}

static JSFunctionSpec MysqlGeneratedKeys_functions[] = {
	JS_FS("getNumber", MysqlGeneratedKeys_getNumber, 1, 0),
	JS_FS("getString", MysqlGeneratedKeys_getString, 1, 0),
	JS_FS("next", MysqlGeneratedKeys_next, 0, 0),
	JS_FS_END
};

static JSBool
Mysql_setStatement(JSContext *cx, unsigned argc, jsval *vp, jsval obj)
{
	JSBool ret = JS_TRUE;
	jsval nativeSQL_argv[] = {JS_ARGV(cx, vp)[0]};
	jsval nativeSQL_jsv;
	jsval conn;

	if (!JS_CallFunctionName(cx, JSVAL_TO_OBJECT(obj), "getConnection",
				argc, vp, &conn)) {
		ret = JS_FALSE;
		JS_Log(JS_LOG_ERR, "Failed to call getConnection\n");
		goto out;
	}

	if (JSVAL_IS_NULL(conn)) {
		ret = JS_FALSE;
		JS_Log(JS_LOG_ERR, "NULL Connection\n");
		goto out;
	}

	if (!JS_CallFunctionName(cx, JSVAL_TO_OBJECT(conn), "nativeSQL", 1,
				nativeSQL_argv, &nativeSQL_jsv)) {
		ret = JS_FALSE;
		JS_Log(JS_LOG_ERR, "Failed to call nativeSQL\n");
		goto out;
	}

	char *nativeSQL = JS_EncodeStringValue(cx, nativeSQL_jsv);
	if (nativeSQL == NULL) {
		ret = JS_FALSE;
		JS_Log(JS_LOG_ERR, "Failed to convert the SQL query\n");
		goto out;
	}

	MYSQL *mysql = (MYSQL *)JS_GetPrivate(JSVAL_TO_OBJECT(conn));
	if (mysql == NULL) {
		free(nativeSQL);
		ret = JS_FALSE;
		JS_Log(JS_LOG_ERR, "Failed to get MYSQL property\n");
		goto out;
	}

	struct prepared_statement *pstmt = malloc(sizeof(struct prepared_statement));
	if (mysql == NULL) {
		free(nativeSQL);
		ret = JS_FALSE;
		JS_Log(JS_LOG_ERR, "Failed to get MYSQL property\n");
		goto out;
	}
	memset(pstmt, 0, sizeof(struct prepared_statement));

	pstmt->stmt = mysql_stmt_init(mysql);
	if (pstmt->stmt == NULL) {
		free(nativeSQL);
		nativeSQL = NULL;
		free(pstmt);
		pstmt = NULL;
		ret = JS_FALSE;
		JS_Log(JS_LOG_ERR, "Failed to initialize statement\n");
		goto out;
	}

	if (mysql_stmt_prepare(pstmt->stmt, nativeSQL, strlen(nativeSQL))) {
		JS_Log(JS_LOG_ERR, "Failed to prepare statement: %s\n", mysql_stmt_error(pstmt->stmt));
		JS_ReportError(cx, "%s", mysql_stmt_error(pstmt->stmt));
		free(nativeSQL);
		nativeSQL = NULL;
		free(pstmt);
		pstmt = NULL;
		ret = JS_FALSE;

		goto out;
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

	JS_SetPrivate(JSVAL_TO_OBJECT(obj), pstmt);
	free(nativeSQL);

out:
	return ret;
}

/* }}} MysqlPreparedGeneratedKeys */

/* {{{ MysqlPreparedResultSet */

static JSClass MysqlPreparedResultSet_class = {
	"MysqlPreparedResultSet", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, NULL,
};

static inline JSBool MysqlPreparedResultSet_get(JSContext *cx, unsigned argc, jsval *vp, struct prepared_statement **pstmt, uint32_t *i, JSBool *result)
{
	jsval this = JS_THIS(cx, vp);
	*pstmt = (struct prepared_statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));
	*result = JS_FALSE;

	if (pstmt == NULL)
		goto out_false;

	if (argc < 1)
		goto out_false;

	if (!JS_ValueToECMAUint32(cx, JS_ARGV(cx, vp)[0], i))
		goto out_false;

	if (*i < 1 || *i > (*pstmt)->r_len)
		goto out_false;

	(*i)--;
	if ((*pstmt)->r_is_null[*i]) {
		*result = JS_TRUE;
		goto out_false;
	}

	return JS_TRUE;

out_false:
	JS_SET_RVAL(cx, vp, JSVAL_NULL);
	return JS_FALSE;
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
		JS_ReportError(cx, "%s", mysql_stmt_error(pstmt->stmt));
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
		JS_ReportError(cx, "%s", mysql_stmt_error(pstmt->stmt));
		return JS_FALSE;
	}

	size_t jslen;

	/* First determine the necessary size for the js buffer.
	 * This is well documented in jsapi.h, just before the
	 * prototype for JS_EncodeCharacters().
	 */
	if (JS_DecodeBytes(cx, cbuf, pstmt->r_bind_len[i], NULL, &jslen) == JS_FALSE)
		return JS_FALSE;
	jschar *jsbuf = JS_malloc(cx, (jslen + 1) * sizeof(jschar));
	assert(jsbuf);

	if (JS_DecodeBytes(cx, cbuf, pstmt->r_bind_len[i], jsbuf, &jslen) == JS_FALSE)
		return JS_FALSE;
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
	if (pstmt == NULL) {
		JS_Log(JS_LOG_ERR, "The statement property is not set\n");
		return JS_FALSE;
	}

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
		JS_ReportError(cx, "%s", mysql_stmt_error(pstmt->stmt));
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

/**
 * MysqlPreparedStatement_finalize - cleanup function for PreparedStatement objects
 * @cx: JavaScript context
 * @obj: object
 */
static void MysqlPreparedStatement_finalize(JSFreeOp *fop, JSObject *obj)
{
	struct prepared_statement *pstmt = (struct prepared_statement *)JS_GetPrivate(obj);

	if (pstmt)
		clear_statement(pstmt);
}

static JSBool prepared_statement_execute(JSContext *cx, struct prepared_statement *pstmt)
{
	unsigned int i;

	if (pstmt->p_len && mysql_stmt_bind_param(pstmt->stmt, pstmt->p_bind)) {
		JS_ReportError(cx, "%s", mysql_stmt_error(pstmt->stmt));
		return JS_FALSE;
	}

	if (mysql_stmt_execute(pstmt->stmt)) {
		JS_ReportError(cx, "%s", mysql_stmt_error(pstmt->stmt));
		clear_statement(pstmt);
		return JS_FALSE;
	}

	for (i = 0; i < pstmt->p_len; i++)
		if (pstmt->p_bind[i].buffer)
			free(pstmt->p_bind[i].buffer);

	free(pstmt->p_bind);
	pstmt->p_bind = NULL;

	if (pstmt->r_len && mysql_stmt_bind_result(pstmt->stmt, pstmt->r_bind)) {
		JS_ReportError(cx, "%s", mysql_stmt_error(pstmt->stmt));
		clear_statement(pstmt);
		return JS_FALSE;
	}

	return JS_TRUE;
}

static JSBool prepared_statement_get_result_set(JSContext *cx, struct prepared_statement *pstmt, jsval *rval)
{
	*rval = JSVAL_NULL;

	JSObject *robj = JS_NewObject(cx, &MysqlPreparedResultSet_class, NULL, NULL);
	if (robj == NULL) {
		JS_ReportError(cx, "Failed to create a new object\n");
		JS_Log(JS_LOG_ERR, "Failed to create a new object\n");
		goto out;
	}

	JS_SetPrivate(robj, pstmt);

	if (JS_DefineFunctions(cx, robj, MysqlPreparedResultSet_functions) == JS_FALSE) {
		JS_ReportError(cx, "Failed to define functions\n");
		JS_Log(JS_LOG_ERR, "Failed to define functions\n");
		goto out;
	}

	*rval = OBJECT_TO_JSVAL(robj);

out:
	return JS_TRUE;
}

static JSBool MysqlStatement_execute(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval this = JS_THIS(cx, vp);
	jsval ret = JSVAL_TRUE;

	if (argc < 0 || argc > 2) {
		JS_Log(JS_LOG_WARNING, "Wrong number of arguments\n");
		ret = JSVAL_FALSE;
		goto out;
	}

	if (argc > 0) {
		/* if there is an old statement clear the memory */
		struct prepared_statement *pstmt =
			(struct prepared_statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));
		if (pstmt)
			clear_statement(pstmt);

		if (Mysql_setStatement(cx, argc, vp, this) == JS_FALSE) {
			ret = JSVAL_FALSE;
			goto out;
		}
	}

	struct prepared_statement *pstmt = (struct prepared_statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));

	if (pstmt == NULL) {
		JS_Log(JS_LOG_ERR, "The statement property is not set\n");
		return JS_FALSE;
	}

	if (!prepared_statement_execute(cx, pstmt))
		return JS_FALSE;

	ret = BOOLEAN_TO_JSVAL(pstmt->r_len > 0);

out:
	JS_SET_RVAL(cx, vp, ret);
	return JS_TRUE;
}

static JSBool MysqlStatement_executeQuery(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval this = JS_THIS(cx, vp);

	/* if it is a simple statement set the SQL command*/
	if (argc == 1) {
		/* if there is an old statement clear the memory */
		struct prepared_statement *pstmt =
			(struct prepared_statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));
		if (pstmt)
			clear_statement(pstmt);

		if (Mysql_setStatement(cx, argc, vp, this) == JS_FALSE) {
			JS_SET_RVAL(cx, vp, JSVAL_NULL);
			goto out;
		}
	}

	struct prepared_statement *pstmt = (struct prepared_statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));

	if (pstmt == NULL) {
		JS_Log(JS_LOG_ERR, "The statement property is not set\n");
		goto out;
	}

	if (!prepared_statement_execute(cx, pstmt))
		goto out;

	prepared_statement_get_result_set(cx, pstmt, &JS_RVAL(cx, vp));
	return JS_TRUE;

out:
	JS_SET_RVAL(cx, vp, JSVAL_NULL);
	return JS_TRUE;
}

static JSBool MysqlStatement_executeUpdate(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval this = JS_THIS(cx, vp);
	jsval rval = JS_NumberValue(-1);

	/* if it is a simple statement set the SQL command*/
	if (argc > 0) {
		/* if there is an old statement clear the memory */
		struct prepared_statement *pstmt =
			(struct prepared_statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));
		if (pstmt)
			clear_statement(pstmt);

		if (Mysql_setStatement(cx, argc, vp, this) == JS_FALSE) {
			goto out;
		}
	}

	struct prepared_statement *pstmt = (struct prepared_statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));

	if (pstmt == NULL) {
		JS_Log(JS_LOG_ERR, "The statement property is not set\n");
		goto out;
	}

	if (!prepared_statement_execute(cx, pstmt))
		goto out;

	my_ulonglong rows = mysql_stmt_affected_rows(pstmt->stmt);
	rval = JS_NumberValue(rows);

out:
	JS_SET_RVAL(cx, vp, rval);
	return JS_TRUE;
}

static JSBool MysqlStatement_getGeneratedKeys(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval this = JS_THIS(cx, vp);
	jsval rval = JSVAL_NULL;
	struct prepared_statement *pstmt = (struct prepared_statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));

	if (pstmt == NULL) {
		JS_Log(JS_LOG_ERR, "The statement property is not set\n");
		return JS_FALSE;
	}

	struct generated_keys *priv = malloc(sizeof(struct generated_keys));
	assert(priv);

	priv->last_insert_id = mysql_stmt_insert_id(pstmt->stmt);
	priv->cursor = 0;

	JSObject *robj = JS_NewObject(cx, &MysqlGeneratedKeys_class, NULL, NULL);
	if (robj == NULL) {
		JS_ReportError(cx, "Failed to create a new object\n");
		JS_Log(JS_LOG_ERR, "Failed to create a new object\n");
		goto out;
	}

	JS_SetPrivate(robj, priv);
	if (JS_DefineFunctions(cx, robj, MysqlGeneratedKeys_functions) == JS_FALSE) {
		JS_ReportError(cx, "Failed to define functions for MysqlGeneratedKeys\n");
		JS_Log(JS_LOG_ERR, "Failed to define functions for MysqlGeneratedKeys\n");
		goto out;
	}

	rval = OBJECT_TO_JSVAL(robj);

out:
	JS_SET_RVAL(cx, vp, rval);
	return JS_TRUE;
}

static JSBool MysqlStatement_getResultSet(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval this = JS_THIS(cx, vp);
	struct prepared_statement *pstmt = (struct prepared_statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));

	if (pstmt == NULL) {
		JS_Log(JS_LOG_ERR, "The statement property is not set\n");
		JS_SET_RVAL(cx, vp, JSVAL_NULL);
		return JS_TRUE;
	}

	if (!pstmt->r_len) {
		JS_SET_RVAL(cx, vp, JSVAL_NULL);
		return JS_TRUE;
	}

	prepared_statement_get_result_set(cx, pstmt, &JS_RVAL(cx, vp));
	return JS_TRUE;
}

static JSBool MysqlStatement_getUpdateCount(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval this = JS_THIS(cx, vp);
	struct prepared_statement *pstmt = (struct prepared_statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));
	my_ulonglong rows = -1;

	if (pstmt == NULL) {
		JS_Log(JS_LOG_ERR, "The statement property is not set\n");
		goto out;
	}

	if (pstmt->r_len)
		rows = mysql_stmt_affected_rows(pstmt->stmt);

out:
	JS_SET_RVAL(cx, vp, JS_NumberValue(rows));
	return JS_TRUE;
}

static JSBool MysqlStatement_getConnection(JSContext *cx, unsigned argc, jsval *vp)
{
	return getConnection(cx, vp);
}

static JSFunctionSpec MysqlStatement_functions[] = {
	JS_FS("execute", MysqlStatement_execute, 2, 0),
	JS_FS("executeQuery", MysqlStatement_executeQuery, 1, 0),
	JS_FS("executeUpdate", MysqlStatement_executeUpdate, 2, 0),
	JS_FS("getConnection", MysqlStatement_getConnection, 0, 0),
	JS_FS("getGeneratedKeys", MysqlStatement_getGeneratedKeys, 0, 0),
	JS_FS("getResultSet", MysqlStatement_getResultSet, 0, 0),
	JS_FS("getUpdateCount", MysqlStatement_getUpdateCount, 0, 0),
	JS_FS_END
};

static JSClass MysqlStatement_class = {
	"MysqlStatement", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, MysqlPreparedStatement_finalize,
};

static inline JSBool MysqlPreparedStatement_set(JSContext *cx, unsigned argc, jsval *vp, struct prepared_statement **pstmt, uint32_t *i, JSBool *result)
{
	jsval this = JS_THIS(cx, vp);
	*pstmt = (struct prepared_statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));
	*result = JS_FALSE;

	if (pstmt == NULL) {
		JS_Log(JS_LOG_ERR, "The statement property is not set\n");
		goto out_false;
	}

	if (argc < 1)
		goto out_false;

	if (!JS_ValueToECMAUint32(cx, JS_ARGV(cx, vp)[0], i))
		goto out_false;

	if (*i < 1 || *i > (*pstmt)->p_len)
		goto out_false;

	(*i)--;
	if ((*pstmt)->p_bind[*i].buffer) {
		// free previously assigned value (buffer)
		free((*pstmt)->p_bind[*i].buffer);
		// prevent second free in prepared_statement_execute()
		// if we never set any other value later
		(*pstmt)->p_bind[*i].buffer = NULL;
	}
	if (argc < 2 || JSVAL_IS_NULL(JS_ARGV(cx, vp)[1])) {
		(*pstmt)->p_bind[*i].buffer_type = MYSQL_TYPE_NULL;
		*result = JS_TRUE;
		goto out_false;
	}

	return JS_TRUE;

out_false:
	JS_SET_RVAL(cx, vp, JSVAL_NULL);
	return JS_FALSE;
}

static JSBool MysqlPreparedStatement_setNumber(JSContext *cx, unsigned argc, jsval *vp)
{
	JSBool ret;
	struct prepared_statement *pstmt;
	uint32_t i;

	if (!MysqlPreparedStatement_set(cx, argc, vp, &pstmt, &i, &ret))
		return ret;

	double val;
	if (!JS_ValueToNumber(cx, JS_ARGV(cx, vp)[1], &val)) {
		JS_SET_RVAL(cx, vp, JSVAL_NULL);
		return JS_FALSE;
	}

	double *buf = malloc(sizeof(double));
	assert(buf);
	*buf = val;

	pstmt->p_bind[i].buffer_type = MYSQL_TYPE_DOUBLE;
	pstmt->p_bind[i].buffer = buf;
	pstmt->p_bind[i].buffer_length = 0;

	JS_SET_RVAL(cx, vp, JSVAL_NULL);
	return JS_TRUE;
}

static JSBool MysqlPreparedStatement_setString(JSContext *cx, unsigned argc, jsval *vp)
{
	JSBool ret;
	struct prepared_statement *pstmt;
	uint32_t i;

	if (!MysqlPreparedStatement_set(cx, argc, vp, &pstmt, &i, &ret))
		return ret;

	JSString *str = JS_ValueToString(cx, JS_ARGV(cx, vp)[1]);

	size_t jslen, clen;
	const jschar *jsbuf = JS_GetStringCharsAndLength(cx, str, &jslen);

	/* First determine the necessary size for the js buffer.
	 * This is well documented in jsapi.h, just before the
	 * prototype for JS_EncodeCharacters().
	 */
	if (JS_EncodeCharacters(cx, jsbuf, jslen, NULL, &clen) == JS_FALSE)
		return ret;

	char *cbuf = malloc(clen);
	assert(cbuf);

	if (JS_EncodeCharacters(cx, jsbuf, jslen, cbuf, &clen) == JS_FALSE) {
		free(cbuf);
		return ret;
	}

	pstmt->p_bind[i].buffer_type = MYSQL_TYPE_STRING;
	pstmt->p_bind[i].buffer = cbuf;
	pstmt->p_bind[i].buffer_length = clen;

	JS_SET_RVAL(cx, vp, JSVAL_NULL);
	return JS_TRUE;
}

static JSFunctionSpec MysqlPreparedStatement_functions[] = {
	JS_FS("execute", MysqlStatement_execute, 0, 0),
	JS_FS("executeQuery", MysqlStatement_executeQuery, 0, 0),
	JS_FS("executeUpdate", MysqlStatement_executeUpdate, 0, 0),
	JS_FS("getConnection", MysqlStatement_getConnection, 0, 0),
	JS_FS("getGeneratedKeys", MysqlStatement_getGeneratedKeys, 0, 0),
	JS_FS("getResultSet", MysqlStatement_getResultSet, 0, 0),
	JS_FS("getUpdateCount", MysqlStatement_getUpdateCount, 0, 0),
	JS_FS("setNumber", MysqlPreparedStatement_setNumber, 2, 0),
	JS_FS("setString", MysqlPreparedStatement_setString, 2, 0),
	JS_FS_END
};

static JSClass MysqlPreparedStatement_class = {
	"MysqlPreparedStatement", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, MysqlPreparedStatement_finalize,
};

/* }}} MysqlPreparedStatement */

/* {{{ MysqlConnection */

/**
 * MysqlConnection_finalize - cleanup function for MysqlConnection objects
 * @cx: JavaScript context
 * @obj: object
 */
static void MysqlConnection_finalize(JSFreeOp *fop, JSObject *obj)
{
	MYSQL *mysql = (MYSQL *)JS_GetPrivate(obj);

	if (mysql) {
		mysql_close(mysql);
		mysql = NULL;
	}
}

static JSClass MysqlConnection_class = {
	"MysqlConnection", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, MysqlConnection_finalize,
};

static JSBool MysqlConnection_createStatement(JSContext *cx, unsigned argc, jsval *vp)
{
	return createStatement(cx, vp, &MysqlStatement_class, MysqlStatement_functions);
}

static JSBool MysqlConnection_nativeSQL(JSContext *cx, unsigned argc, jsval *vp)
{
	return nativeSQL(cx, argc, vp);
}

static JSBool MysqlConnection_prepareStatement(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval rval = JSVAL_NULL;

	if (argc < 1 || argc > 2) {
		JS_Log(JS_LOG_WARNING, "Wrong number of arguments\n");
		goto out;
	}

	JSObject *robj = JS_NewObject(cx, &MysqlPreparedStatement_class, NULL, NULL);
	if (robj == NULL) {
		JS_ReportError(cx, "Failed to create a new object\n");
		JS_Log(JS_LOG_ERR, "Failed to create a new object\n");
		goto out;
	}

	JS_DefineProperty(cx, robj, "connection", JS_THIS(cx, vp), NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);
	if (JS_DefineFunctions(cx, robj, MysqlPreparedStatement_functions) == JS_FALSE) {
		JS_ReportError(cx, "Failed to define functions\n");
		JS_Log(JS_LOG_ERR, "Failed to define functions\n");
		goto out;
	}

	if (Mysql_setStatement(cx, argc, vp, OBJECT_TO_JSVAL(robj)) == JS_FALSE)
		goto out;

	rval = OBJECT_TO_JSVAL(robj);

out:
	JS_SET_RVAL(cx, vp, rval);
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
	char *url = JS_EncodeStringValue(cx, JS_ARGV(cx, vp)[0]);

	JS_Log(JS_LOG_INFO, "mysql check: '%s'\n", url);
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

	/* If we can't parse the URL, just return NULL (but no error) to
	 * allow the DriverManager code to try the next driver */
	ret = JS_TRUE;

	char *url = JS_EncodeStringValue(cx, JS_ARGV(cx, vp)[0]);
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
		JS_ReportError(cx, "%s", mysql_error(mysql));
		mysql_close(mysql);
		goto out_clean;
	}

	if (mysql_set_character_set(mysql, "utf8") != 0) {
		ret = JS_FALSE;
		JS_ReportError(cx, "Failed to set the character set for the connector\n");
		JS_Log(JS_LOG_ERR, "Failed to set the character set for the connector\n");
		goto out_clean;
	}

	/* Connection is successful. Create a new connection object and
	 * link the mysql object to it. */
	JSObject *robj =  JS_NewObject(cx, &MysqlConnection_class, NULL, NULL);
	if (robj == NULL) {
		ret = JS_FALSE;
		JS_ReportError(cx, "Failed to create a new object\n");
		JS_Log(JS_LOG_ERR, "Failed to create a new object\n");
		goto out_clean;
	}

	JS_SetPrivate(robj, mysql);

	if (JS_DefineFunctions(cx, robj, MysqlConnection_functions) == JS_FALSE) {
		ret = JS_FALSE;
		JS_ReportError(cx, "Failed to define functions for MysqlConnection class\n");
		JS_Log(JS_LOG_ERR, "Failed to define functions\n");
		goto out_clean;
	}
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
