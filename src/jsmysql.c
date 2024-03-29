/* SPDX-License-Identifier: MIT */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <mysql.h>
#include <math.h>
#include <assert.h>
#include <jsmisc.h>

#include "config.h"
#include "jsmysql.h"
#include "jssql.h"
#include "jscommon.h"

/*
 * MySQL backend driver - implementation details
 * =============================================
 *
 * MySQL C API usage
 * -----------------
 *
 * The MySQL C API consists of two different sets of functions for query and
 * result manipulation. They correspond to simple and prepared statements,
 * respectively.
 *
 * Examples of simple statement functions:
 *   mysql_query()
 *   mysql_num_fields()
 *   mysql_use_result()
 *   mysql_store_result()
 *   mysql_fetch_row()
 *
 * Example of prepared statement functions:
 *   mysql_stmt_prepare()
 *   mysql_stmt_bind_result()
 *   mysql_stmt_execute()
 *   mysql_stmt_fetch()
 *
 * While these two sets of functions seem to map well to the Statement and
 * PreparedStatement classes, the former has a limitation with regards to
 * running multiple queries simultaneously using the same connection to the
 * MySQL server. To start another query, the results for the previous query
 * must be either retrieved up to the last one or stored locally, and this
 * does not work well with large data sets. To overcome the limitation, we
 * use the prepared statement C API to implement both types of statement
 * classes. From the Java SQL API perspective, there is no restriction to
 * create two or more Statement objects from the same Connection object at
 * the same time, and retrieve results sequentially. We aim to support this
 * use case as well.
 *
 * The `my_bool` type
 * ------------------
 *
 * In recent versions of MySQL, the `my_bool` type no longer exists, but in
 * MariaDB it still exists. We want our code to be compatible with both, so
 * we use autoconf to detect whether the `my_bool` type is defined and alias
 * it to bool otherwise.
 */

struct prepared_statement {
	MYSQL_STMT *stmt;

	/* parameters */
	MYSQL_BIND *p_bind;
	unsigned int p_len;

	/* results */
	MYSQL_BIND *r_bind;
	unsigned int r_len;
	unsigned long *r_bind_len;
	my_bool *r_is_null;

	/* generated keys */
	bool return_generated_keys;
};

struct generated_keys {
	my_ulonglong last_insert_id;
	my_ulonglong affected_rows;
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

static void execute_statement(duk_context *ctx, struct prepared_statement *pstmt)
{
	unsigned int i;

	if (pstmt->p_len && mysql_stmt_bind_param(pstmt->stmt, pstmt->p_bind))
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", mysql_stmt_error(pstmt->stmt));

	if (mysql_stmt_execute(pstmt->stmt)) {
		clear_statement(pstmt);
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", mysql_stmt_error(pstmt->stmt));
	}

	for (i = 0; i < pstmt->p_len; i++)
		if (pstmt->p_bind[i].buffer)
			free(pstmt->p_bind[i].buffer);

	free(pstmt->p_bind);
	pstmt->p_bind = NULL;

	if (pstmt->r_len && mysql_stmt_bind_result(pstmt->stmt, pstmt->r_bind)) {
		clear_statement(pstmt);
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", mysql_stmt_error(pstmt->stmt));
	}
}

static bool return_generated_keys(duk_context *ctx)
{
	int argc = duk_get_top(ctx);

	if (argc == 1)
		return false;

	if (duk_is_number(ctx, 1)) {
		if (duk_get_number(ctx, 1) == 0)
			return false;

		return true;
	}

	return true;
}


static int set_statement(duk_context *ctx, const char *query, bool generated_keys)
{
	const char *nativeSQL;
	unsigned int i;
	const char *error_message;

	/* Call nativeSQL method on the connection object in the PreparedStatement given */
	duk_get_prop_string(ctx, -1, "connection");
	if (duk_is_undefined(ctx, -1)) {
		duk_pop(ctx);
		return 0;
	}

	duk_push_string(ctx, "nativeSQL");
	duk_push_string(ctx, query);
	duk_pcall_prop(ctx, -3, 1);
	nativeSQL = duk_get_string(ctx, -1);
	duk_pop(ctx);

	duk_get_prop_string(ctx, -1, "connection");
	MYSQL *mysql = (MYSQL *)duk_get_pointer(ctx, -1);
	duk_pop(ctx);
	if (mysql == NULL)
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", "MYSQL property is not set\n");

	struct prepared_statement *pstmt = malloc(sizeof(struct prepared_statement));

	memset(pstmt, 0, sizeof(struct prepared_statement));

	pstmt->stmt = mysql_stmt_init(mysql);
	if (pstmt->stmt == NULL) {
		free(pstmt);
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s","Failed to initialize statement\n");
	}

	if (mysql_stmt_prepare(pstmt->stmt, nativeSQL, strlen(nativeSQL))) {
		error_message = mysql_stmt_error(pstmt->stmt);
		mysql_stmt_free_result(pstmt->stmt);
		free(pstmt);
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", error_message);
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

	for (i = 0; i < pstmt->r_len; i++) {
		pstmt->r_bind[i].length = &pstmt->r_bind_len[i];
		pstmt->r_bind[i].is_null = &pstmt->r_is_null[i];
	}

	pstmt->return_generated_keys = generated_keys;

	/* Remove the connection object */
	duk_pop(ctx);

	/* Add property to the given PreparedStatement object */
	duk_push_pointer(ctx, (void *) pstmt);
	duk_put_prop_string(ctx, -2, "pstmt");

	return 1;
}

static int validate_column_index(duk_context *ctx, struct prepared_statement **pstmt, uint32_t *i)
{
	int argc = duk_get_top(ctx);

	duk_push_this(ctx);
	duk_get_prop_string(ctx, -1, "pstmt");
	*pstmt = (struct prepared_statement *)duk_get_pointer(ctx, -1);

	if (pstmt == NULL)
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", "The statement property is not set\n");

	if (argc < 1)
		return 0;

	*i = duk_get_int(ctx, 0);

	if (*i < 1 || *i > (*pstmt)->r_len)
		return 0;

	(*i)--;
	if ((*pstmt)->r_is_null[*i])
		return 0;

	return 1;
}

static int validate_paramater_index(duk_context *ctx, struct prepared_statement **pstmt, uint32_t *i)
{
	int argc = duk_get_top(ctx);

	duk_push_this(ctx);
	duk_get_prop_string(ctx, -1, "pstmt");
	*pstmt = (struct prepared_statement *)duk_get_pointer(ctx, -1);

	if (pstmt == NULL)
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", "The statement property is not set\n");

	if (argc < 1)
		return 0;

	*i = duk_get_int(ctx, 0);

	if (*i < 1 || *i > (*pstmt)->p_len)
		return 0;

	(*i)--;
	if ((*pstmt)->p_bind[*i].buffer) {
		/* Free previously assigned value (buffer) */
		free((*pstmt)->p_bind[*i].buffer);
		/* Prevent second free in execute_statement()
		if we never set any other value later */
		(*pstmt)->p_bind[*i].buffer = NULL;
	}
	if (argc < 2 || duk_is_null(ctx, 1)) {
		(*pstmt)->p_bind[*i].buffer_type = MYSQL_TYPE_NULL;
		return 0;
	}

	return 1;
}

/* {{{ MysqlPreparedGeneratedKeys */

static int MysqlGeneratedKeys_getNumber(duk_context *ctx)
{
	duk_push_this(ctx);
	duk_get_prop_string(ctx, -1, "generatedKeys");
	struct generated_keys *k = duk_get_pointer(ctx, -1);

	if (k)
		duk_push_int(ctx, k->last_insert_id);
	else
		duk_push_null(ctx);

	return 1;
}

static int MysqlGeneratedKeys_getString(duk_context *ctx)
{
	duk_push_this(ctx);
	duk_get_prop_string(ctx, -1, "generatedKeys");
	struct generated_keys *k = duk_get_pointer(ctx, -1);
	char *cstr;

	if (k == NULL) {
		duk_push_null(ctx);
		return 1;
	}

	if (asprintf(&cstr, "%ju", (uintmax_t)k->last_insert_id) == -1)
		return DUK_RET_ERROR;

	duk_push_string(ctx, cstr);
	free(cstr);

	return 1;
}

static int MysqlGeneratedKeys_next(duk_context *ctx)
{
	duk_push_this(ctx);
	duk_get_prop_string(ctx, -1, "generatedKeys");
	struct generated_keys *k = duk_get_pointer(ctx, -1);

	if (k == NULL)
		duk_push_false(ctx);

	if (k->cursor >0)
		k->last_insert_id++;

	if (k->cursor < k->affected_rows) {
		k->cursor++;
		duk_push_true(ctx);
	} else
		duk_push_false(ctx);

	return 1;
}

static int MysqlGeneratedKeys_finalize(duk_context *ctx)
{
	struct generated_keys *k;
	duk_get_prop_string(ctx, 0, "generatedKeys");

	k = duk_get_pointer(ctx, -1);
	if (k) {
		free(k);
		k = NULL;
	}

	return 0;
}

static duk_function_list_entry MysqlGeneratedKeys_functions[] = {
	{"getNumber",	MysqlGeneratedKeys_getNumber,	1},
	{"getString",	MysqlGeneratedKeys_getString,	1},
	{"next",	MysqlGeneratedKeys_next,	0},
	{NULL,		NULL, 				0}
};

/* }}} MysqlPreparedGeneratedKeys */

/* {{{ MysqlPreparedResultSet */

static int MysqlResultSet_getNumber(duk_context *ctx)
{
	struct prepared_statement *pstmt;
	uint32_t i;
	MYSQL_BIND bind;
	double val;

	if(!validate_column_index(ctx, &pstmt, &i))
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", "Index is not valid\n");

	memset(&bind, 0, sizeof(MYSQL_BIND));
	bind.buffer_type = MYSQL_TYPE_DOUBLE;
	bind.buffer = &val;
	bind.buffer_length = sizeof(double);

	if (mysql_stmt_fetch_column(pstmt->stmt, &bind, i, 0))
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", mysql_stmt_error(pstmt->stmt));

	duk_push_int(ctx, val);
	return 1;
}

static int MysqlResultSet_getString(duk_context *ctx)
{
	struct prepared_statement *pstmt;
	uint32_t i;
	MYSQL_BIND bind;
	char *cbuf;

	if(!validate_column_index(ctx, &pstmt, &i))
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", "Index is not valid\n");

	if (!pstmt->r_bind_len[i]) {
		duk_push_string(ctx, "");
		return 1;
	}

	cbuf = malloc(pstmt->r_bind_len[i]);
	assert(cbuf);

	memset(&bind, 0, sizeof(MYSQL_BIND));
	bind.buffer_type = MYSQL_TYPE_STRING;
	bind.buffer = cbuf;
	bind.buffer_length = pstmt->r_bind_len[i];

	if (mysql_stmt_fetch_column(pstmt->stmt, &bind, i, 0))
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", mysql_stmt_error(pstmt->stmt));

	duk_push_lstring(ctx, cbuf, pstmt->r_bind_len[i]);
	return 1;
}

static int MysqlResultSet_next(duk_context *ctx)
{
	duk_push_this(ctx);
	duk_get_prop_string(ctx, -1, "pstmt");
	struct prepared_statement *pstmt = duk_get_pointer(ctx, -1);
	if (pstmt == NULL)
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", "The statement property is not set\n");

	switch (mysql_stmt_fetch(pstmt->stmt)) {
	case 0:
	case MYSQL_DATA_TRUNCATED:
		duk_push_true(ctx);
		break;
	case MYSQL_NO_DATA:
		duk_push_false(ctx);
		break;
	default:
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", mysql_stmt_error(pstmt->stmt));
	}

	return 1;
}

static duk_function_list_entry MysqlResultSet_functions[] = {
	{"getNumber",	MysqlResultSet_getNumber,	1},
	{"getString",	MysqlResultSet_getString,	1},
	{"next",	MysqlResultSet_next,		0},
	{NULL,		NULL, 				0}
};

#if 0

/* }}} MysqlPreparedResultSet */

/* {{{ MysqlPreparedStatement */

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

#endif

static int MysqlStatement_execute(duk_context *ctx)
{
	int argc = duk_get_top(ctx);
	bool generated_keys;

	if (argc < 1 || argc > 2)
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", "Wrong number of arguments\n");

	generated_keys = return_generated_keys(ctx);

	duk_push_this(ctx);
	duk_get_prop_string(ctx, -1, "pstmt");
	struct prepared_statement *pstmt = duk_get_pointer(ctx, -1);

	if (pstmt)
		clear_statement(pstmt);

	/* Must push the Statement object on the stack as it is used by
	set_statement function */
	duk_push_this(ctx);
	if (!set_statement(ctx, duk_get_string(ctx, 0), generated_keys)) {
		/* TODO error */
		return DUK_RET_ERROR;
	}

	duk_get_prop_string(ctx, -1, "pstmt");
	pstmt = duk_get_pointer(ctx, -1);
	if (pstmt == NULL)
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", "The statement property is not set\n");

	execute_statement(ctx, pstmt);

	duk_push_boolean(ctx, pstmt->r_len > 0);

	return 1;
}

static int MysqlStatement_executeQuery(duk_context *ctx)
{
	int argc = duk_get_top(ctx);

	if (argc != 1) 
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", "Wrong number of arguments\n");

	duk_push_this(ctx);
	duk_get_prop_string(ctx, -1, "pstmt");
	struct prepared_statement *pstmt = duk_get_pointer(ctx, -1);

	if (pstmt)
		clear_statement(pstmt);

	/* Must push the Statement object on the stack as it is used by
	set_statement function */
	duk_push_this(ctx);
	if (!set_statement(ctx, duk_get_string(ctx, 0), false)) {
		/* TODO error */
		return DUK_RET_ERROR;
	}

	duk_get_prop_string(ctx, -1, "pstmt");
	pstmt = duk_get_pointer(ctx, -1);
	if (pstmt == NULL)
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", "The statement property is not set\n");

	execute_statement(ctx, pstmt);


	/* Create MySQL Result Set object */
	duk_push_object(ctx);

	duk_get_global_string(ctx, "MysqlResultSet");
	duk_set_prototype(ctx, -2);

	duk_push_pointer(ctx, (void *) pstmt);
	duk_put_prop_string(ctx, -2, "pstmt");

	return 1;
}

static int MysqlStatement_executeUpdate(duk_context *ctx)
{
	int argc = duk_get_top(ctx);
	bool generated_keys;

	if (argc < 1 || argc > 2)
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", "Wrong number of arguments\n");

	generated_keys = return_generated_keys(ctx);

	duk_push_this(ctx);
	duk_get_prop_string(ctx, -1, "pstmt");
	struct prepared_statement *pstmt = duk_get_pointer(ctx, -1);

	if(pstmt)
		clear_statement(pstmt);

	/* Must push the Statement object on the stack as it is used by
	set_statement function */
	duk_push_this(ctx);
	if (!set_statement(ctx, duk_get_string(ctx, 0), generated_keys)) {
		/* TODO error */
		return DUK_RET_ERROR;
	}

	duk_get_prop_string(ctx, -1, "pstmt");
	pstmt = duk_get_pointer(ctx, -1);
	if (pstmt == NULL)
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", "The statement property is not set\n");

	execute_statement(ctx, pstmt);

	my_ulonglong rows = mysql_stmt_affected_rows(pstmt->stmt);
	duk_push_number(ctx, rows);

	return 1;
}

static int MysqlStatement_getConnection(duk_context *ctx)
{
	duk_push_this(ctx);

	duk_get_prop_string(ctx, -1, "connection");

	return 1;
}

static int MysqlStatement_getGeneratedKeys(duk_context *ctx)
{
	duk_push_this(ctx);
	duk_get_prop_string(ctx, -1, "pstmt");
	struct prepared_statement *pstmt = duk_get_pointer(ctx, -1);

	if (pstmt == NULL)
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", "The statement property is not set\n");

	if (pstmt->return_generated_keys == false)
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", "Generated keys were not selected to be retrieved\n");

	struct generated_keys *priv = malloc(sizeof(struct generated_keys));
	assert(priv);

	priv->affected_rows = mysql_stmt_affected_rows(pstmt->stmt);
	priv->last_insert_id = mysql_stmt_insert_id(pstmt->stmt);

	priv->cursor = 0;

	/* Create MySQL Generated keys object */
	duk_push_object(ctx);

	duk_get_global_string(ctx, "MysqlGeneratedKeys");
	duk_set_prototype(ctx, -2);

	duk_push_pointer(ctx, (void *) priv);
	duk_put_prop_string(ctx, -2, "generatedKeys");

	return 1;
}

static int MysqlStatement_getResultSet(duk_context *ctx)
{
	duk_push_this(ctx);
	duk_get_prop_string(ctx, -1, "pstmt");
	struct prepared_statement *pstmt = duk_get_pointer(ctx, -1);

	if (pstmt == NULL)
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", "The statement property is not set\n");

	if (!pstmt->r_len)
		duk_push_null(ctx);

	/* Create MySQL Result set object */
	duk_push_object(ctx);

	duk_get_global_string(ctx, "MysqlPreparedStatement");
	duk_set_prototype(ctx, -2);

	duk_push_pointer(ctx, (void *) pstmt);
	duk_put_prop_string(ctx, -2, "pstmt");

	return 1;
}

static int MysqlStatement_getUpdateCount(duk_context *ctx)
{
	my_ulonglong rows = -1;

	duk_push_this(ctx);
	duk_get_prop_string(ctx, -1, "pstmt");
	struct prepared_statement *pstmt = duk_get_pointer(ctx, -1);

	if (pstmt == NULL)
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", "The statement property is not set\n");

	if (pstmt->r_len)
		rows = mysql_stmt_affected_rows(pstmt->stmt);

	duk_push_number(ctx, rows);

	return 1;
}

static int MysqlStatement_finalize(duk_context *ctx)
{
	struct prepared_statement *pstmt;
	duk_get_prop_string(ctx, 0, "pstmt");

	pstmt = duk_get_pointer(ctx, -1);

	if (pstmt)
		clear_statement(pstmt);

	return 0;
}

static duk_function_list_entry MysqlStatement_functions[] = {
	{"execute",		MysqlStatement_execute,			DUK_VARARGS},
	{"executeQuery",	MysqlStatement_executeQuery,		DUK_VARARGS},
	{"executeUpdate",	MysqlStatement_executeUpdate,		DUK_VARARGS},
	{"getConnection",	MysqlStatement_getConnection,		0},
	{"getGeneratedKeys",	MysqlStatement_getGeneratedKeys,	0},
	{"getResultSet",	MysqlStatement_getResultSet,		0},
	{"getUpdateCount",	MysqlStatement_getUpdateCount,		0},
	{NULL,			NULL, 					0}
};

#if 0

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

#endif

static int MysqlPreparedStatement_execute(duk_context *ctx)
{
	int argc = duk_get_top(ctx);

	if (argc != 0)
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", "Wrong number of arguments\n");

	duk_push_this(ctx);
	duk_get_prop_string(ctx, -1, "pstmt");
	struct prepared_statement *pstmt = duk_get_pointer(ctx, -1);

	if (pstmt == NULL)
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", "The statement property is not set\n");

	execute_statement(ctx, pstmt);

	duk_push_boolean(ctx, pstmt->r_len > 0);

	return 1;
}

static int MysqlPreparedStatement_executeQuery(duk_context *ctx)
{
	int argc = duk_get_top(ctx);

	if (argc != 0) 
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", "Wrong number of arguments\n");

	duk_push_this(ctx);
	duk_get_prop_string(ctx, -1, "pstmt");
	struct prepared_statement *pstmt = duk_get_pointer(ctx, -1);

	if (pstmt == NULL)
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", "The statement property is not set\n");

	execute_statement(ctx, pstmt);

	/* Create MySQL Result Set object */
	duk_push_object(ctx);

	duk_get_global_string(ctx, "MysqlResultSet");
	duk_set_prototype(ctx, -2);

	duk_push_pointer(ctx, (void *) pstmt);
	duk_put_prop_string(ctx, -2, "pstmt");

	return 1;
}

static int MysqlPreparedStatement_executeUpdate(duk_context *ctx)
{
	int argc = duk_get_top(ctx);

	if (argc != 0)
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", "Wrong number of arguments\n");

	duk_push_this(ctx);
	duk_get_prop_string(ctx, -1, "pstmt");
	struct prepared_statement *pstmt = duk_get_pointer(ctx, -1);

	if (pstmt == NULL)
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", "The statement property is not set\n");

	execute_statement(ctx, pstmt);

	my_ulonglong rows = mysql_stmt_affected_rows(pstmt->stmt);
	duk_push_number(ctx, rows);

	return 1;
}

static int MysqlPreparedStatement_setNumber(duk_context *ctx)
{
	struct prepared_statement *pstmt;
	uint32_t i;

	if (!validate_paramater_index(ctx, &pstmt, &i))
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", "Index is not valid\n");

	double val = duk_get_number(ctx, 1);
	if (isnan(val))
		return DUK_RET_ERROR;

	double *buf = malloc(sizeof(double));
	assert(buf);
	*buf = val;

	pstmt->p_bind[i].buffer_type = MYSQL_TYPE_DOUBLE;
	pstmt->p_bind[i].buffer = buf;
	pstmt->p_bind[i].buffer_length = 0;

	return 0;
}

static int MysqlPreparedStatement_setString(duk_context *ctx)
{
	struct prepared_statement *pstmt;
	uint32_t i;
	const char *str;
	size_t len;

	if (!validate_paramater_index(ctx, &pstmt, &i))
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", "Index is not valid\n");

	str = duk_to_string(ctx, 1);
	str = duk_get_lstring(ctx, 1, &len);

	pstmt->p_bind[i].buffer_type = MYSQL_TYPE_STRING;
	pstmt->p_bind[i].buffer = malloc(len);
	memcpy(pstmt->p_bind[i].buffer, str, len);
	pstmt->p_bind[i].buffer_length = len;

	return 0;
}

static duk_function_list_entry MysqlPreparedStatement_functions[] = {
	{"execute",		MysqlPreparedStatement_execute,		0},
	{"executeQuery",	MysqlPreparedStatement_executeQuery,		0},
	{"executeUpdate",	MysqlPreparedStatement_executeUpdate,	0},
	{"setNumber",		MysqlPreparedStatement_setNumber,	2},
	{"setString",		MysqlPreparedStatement_setString,	2},
	{NULL,			NULL,					0}
};

/* }}} MysqlPreparedStatement */

/* {{{ MysqlConnection */

static int MysqlConnection_createStatement(duk_context *ctx)
{
	/* Create MySQL Statement object */
	duk_push_object(ctx);

	duk_get_global_string(ctx, "MysqlStatement");
	duk_set_prototype(ctx, -2);

	/* Add connection property */
	duk_push_this(ctx);
	duk_put_prop_string(ctx, -2, "connection");

	return 1;
}

static int MysqlConnection_nativeSQL(duk_context *ctx)
{
	int argc = duk_get_top(ctx);

	if (argc != 1)
		return DUK_RET_ERROR;

	duk_to_string(ctx, 0);

	/* duk_to_string also replaces the value at idx with ToString(val) */
	duk_dup(ctx, 0);

	return 1;
}

static int MysqlConnection_prepareStatement(duk_context *ctx)
{
	int argc = duk_get_top(ctx);
	bool generated_keys;

	if (argc < 1 || argc > 2)
		return DUK_RET_ERROR;

	generated_keys = return_generated_keys(ctx);

	/* Create MySQL Prepared Statement object */
	duk_push_object(ctx);

	duk_get_global_string(ctx, "MysqlPreparedStatement");
	duk_set_prototype(ctx, -2);

	/* Add connection property */
	duk_push_this(ctx);
	duk_put_prop_string(ctx, -2, "connection");

	/* set_statement uses the duktape stack for the PreparedStatement object
	and receives also the query as a normal param. It will simply add the prepared_statement
	struct as a property to the PreparedStatement object.*/
	if (!set_statement(ctx, duk_get_string(ctx, 0), generated_keys)) {
		/* TODO error */
		return DUK_RET_ERROR;
	}

	return 1;
}

static int MysqlConnection_finalize(duk_context *ctx)
{
	MYSQL *mysql;
	duk_get_prop_string(ctx, 0, "connection");
	mysql = duk_get_pointer(ctx, -1);

	if (mysql) {
		mysql_close(mysql);
		mysql = NULL;
	}

	return 0;
}


static duk_function_list_entry MysqlConnection_functions[] = {
	{"createStatement",	MysqlConnection_createStatement,	0},
	{"nativeSQL",		MysqlConnection_nativeSQL,		1},
	{"prepareStatement",	MysqlConnection_prepareStatement,	DUK_VARARGS},
	{NULL,			NULL,					0}
};

/* }}} MysqlConnection */

/* {{{ MysqlDriver */

static int MysqlDriver_acceptsURL(duk_context *ctx)
{
	const char *url = duk_safe_to_string(ctx, 0);

	if (strncmp(url, "mysql://", 8))
		duk_push_false(ctx);
	else
		duk_push_true(ctx);

	return 1;
}

static int MysqlDriver_connect(duk_context *ctx)
{
	int argc = duk_get_top(ctx);
	char *db, *host, *port_str;
	const char *url;
	unsigned int port = 3306;
	const char *user = NULL;
	const char *password = NULL;

	if (!argc)
		return DUK_RET_ERROR;

	url = duk_safe_to_string(ctx, 0);
	/* It is a different kind of driver, so we return null and it can retry with another */
	if (strncmp(url, "mysql://", 8)) {
		duk_push_null(ctx);
		return 1;
	}

	host = strdup(url + 8);
	db = host + strcspn(host, "/:");

	switch (*db) {
	case ':':
		port_str = ++db;
		db += strcspn(db, "/");
		if (*db != '/') {
			free(host);
			duk_push_null(ctx);
			return 1;
		}
		*(db++) = '\0';
		port = atoi(port_str);
		break;
	case '/':
		*(db++) = '\0';
		break;
	default:
		free(host);
		duk_push_null(ctx);
		return 1;
	}

	if (argc > 1 && duk_is_object(ctx, 1)) {

		duk_get_prop_string(ctx, 1, "user");

		if (!duk_is_undefined(ctx, -1))
			user = duk_safe_to_string(ctx, -1);

		duk_get_prop_string(ctx, 1, "password");

		if (!duk_is_undefined(ctx, -1))
			password = duk_safe_to_string(ctx, -1);
	}

	MYSQL *mysql = mysql_init(NULL);
	MYSQL *conn = mysql_real_connect(mysql, host, user, password, db, port, NULL, 0);

	free(host);

	if (!conn) {
		/* This call never returns */
		mysql_close(mysql);
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", mysql_error(mysql));
	}

	if (mysql_set_character_set(mysql, "utf8") != 0) {
		mysql_close(mysql);
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", "Failed to set the character set for the connector\n");
	}

	/* Create Connection object */
	duk_push_object(ctx);

	duk_get_global_string(ctx, "MysqlConnection");
	duk_set_prototype(ctx, -2);

	duk_push_pointer(ctx, (void *) mysql);
	duk_put_prop_string(ctx, -2, "connection");

	return 1;
}

static duk_function_list_entry MysqlDriver_functions[] = {
	{"acceptsURL",	MysqlDriver_acceptsURL,	1},
	{"connect",	MysqlDriver_connect,	2},
	{NULL,		NULL,			0}
};

/* }}} MysqlDriver */

duk_bool_t js_mysql_construct_and_register(duk_context *ctx)
{
	int rc;

	/* Create MysqlGeneratedKeys "class" */
	duk_push_object(ctx);
	duk_put_function_list(ctx, -1, MysqlGeneratedKeys_functions);
	duk_push_c_function(ctx, MysqlGeneratedKeys_finalize, 2);
	duk_set_finalizer(ctx, -2);
	duk_put_global_string(ctx, "MysqlGeneratedKeys");

	/* Create MysqlResultSet "class" */
	duk_push_object(ctx);
	duk_put_function_list(ctx, -1, MysqlResultSet_functions);
	duk_put_global_string(ctx, "MysqlResultSet");

	/* Create MysqlConnection "class" */
	duk_push_object(ctx);
	duk_push_c_function(ctx, MysqlConnection_finalize, 2);
	duk_set_finalizer(ctx, -2);
	duk_put_function_list(ctx, -1, MysqlConnection_functions);
	duk_put_global_string(ctx, "MysqlConnection");

	/* Create MysqlStatement "class" */
	duk_push_object(ctx);

	duk_get_global_string(ctx, "Statement");
	duk_set_prototype(ctx, -2);
	duk_push_c_function(ctx, MysqlStatement_finalize, 2);
	duk_set_finalizer(ctx, -2);
	duk_put_function_list(ctx, -1, MysqlStatement_functions);
	duk_put_global_string(ctx, "MysqlStatement");

	/* Create MysqlPreparedStatement "class" */
	duk_push_object(ctx);

	duk_get_global_string(ctx, "MysqlStatement");
	duk_set_prototype(ctx, -2);

	duk_put_function_list(ctx, -1, MysqlPreparedStatement_functions);
	duk_put_global_string(ctx, "MysqlPreparedStatement");

	/* Create MySQL Driver object */
	duk_push_object(ctx);
	duk_put_function_list(ctx, -1, MysqlDriver_functions);
	duk_put_global_string(ctx, "MysqlDriver");

	/* Register driver to DriverManager */
	duk_get_global_string(ctx, "DriverManager");
	duk_push_string(ctx, "registerDriver");
	duk_get_global_string(ctx, "MysqlDriver");
	rc = duk_pcall_prop(ctx, -3, 1);

	return !rc;
}

// vim: foldmethod=marker
