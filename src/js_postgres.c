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
#include "js_postgres.h"

struct statement {
	char *command;
	int type;			//this can be removed because we can identify the type 
						//of statement using p_len

	// parameters
	uint32_t p_len;
	char **p_values;

	//connection
	PGconn *conn;

	// results
	PGresult *result;
	uint32_t row_index;
};

/**
 * pretty_printer - internal function used for debugging
 * @stmt: pointer to the structure
 */
static void pretty_printer(struct statement *stmt)
{
	unsigned int i;

	dlog(LOG_INFO, "Statement type: %s | Parameters number %d\n", 
		(stmt->type == SIMPLE_STATEMENT)? "SIMPLE_STATEMENT":"PREPARED_STATEMENT", 
		stmt->p_len);
	dlog(LOG_INFO, "Statement :\"%s\"\n", stmt->command);
	
	for (i = 0; i < stmt->p_len; i++) {
		dlog(LOG_INFO, "%d argument: %s\n", i + 1, 
			(stmt->p_values[i] == NULL)? "NULL":stmt->p_values[i]);		
	}
}

/**
 * result_pretty_printer - internal function used for debugging
 * @stmt: pointer to the structure
 */
static void result_pretty_printer(struct statement *stmt)
{
	PQprint(stdout, stmt->result, NULL);
}

/**
 * clear_statement - clears the statement structure
 * @stmt: pointer to the structure
 */
static void clear_statement(struct statement *stmt)
{
	free(stmt->command);
	if(stmt->type == PREPARED_STATEMENT) {
		int i;
		for (i = 0; i < stmt->p_len; i++)
			free(stmt->p_values[i]);
		free(stmt->p_values);
	}
	
	PQclear(stmt->result);
	free(stmt);
}

/**
 * get_plen - counts the parameters' number
 * @nativeSQL: SQL command
 */
static int get_plen(char *nativeSQL)
{
	int i, p_len;

	p_len = 0;

	for (i = 0; nativeSQL[i]; i++) {
		if (nativeSQL[i] == '?')
			p_len++;
	}

	return p_len;
}

/**
 * postgres_pstmt_conversion - converts a prepared statement to a 
 * postgresql's one
 * @dest: the result
 * @source: SQL command which will be converted
 */
static void postgres_pstmt_conversion(char *dest, char *source)
{
	int i;
	i = 1;

	while (*source) {
		if ((*source) == '?') {
			*dest++ = '$';
			char *nr = my_itoa(i);
			int j, len;
			len = strlen(nr);
			for (j = 0; j < len; j++)
				*dest++ = nr[j];
			i++;
			source++;
			free(nr);
		} else {
			*dest++ = *source++;
		}
	}

	*dest = '\0';
}

/**
 * generate_statement - creates a new statement using the SQL command
 * @cx: the JavaScript context
 * @cmd: SQL command
 *
 * Returns a pointer to the statement structure or NULL in case of failure.
 */
static struct statement *generate_statement(JSContext *cx, jsval cmd) {
	struct statement *stmt = malloc(sizeof(struct statement));
	
	if (stmt == NULL) {
		dlog(LOG_ALERT, "Failed to allocate memory for statement structure\n");
		return NULL;
	}
	memset(stmt, 0, sizeof(struct statement));

	char *nativeSQL = JSString_to_CString(cx, cmd);

	stmt->p_len = get_plen(nativeSQL);
	stmt->row_index = 0;
	
	/* it supports maximum 99 parameters */
	if (stmt->p_len > MAX_PARAMETERS) {
		free(stmt);
		return NULL;
	}

	stmt->command = malloc((strlen(nativeSQL) + stmt->p_len + stmt->p_len / 10) *
							sizeof(char));
	if (stmt->command == NULL) {
		free(stmt);
		dlog(LOG_ALERT, "Failed to allocate memory for statement command\n");
		return NULL;
	}

	if (stmt->p_len > 0) {
		stmt->type = PREPARED_STATEMENT;
		stmt->p_values = calloc(stmt->p_len, sizeof(char *));
		if (stmt->p_values == NULL) {
			free(stmt->command);
			free(stmt);
			dlog(LOG_ALERT, "Failed to allocate memory for statement values\n");
			return NULL;
		}
		postgres_pstmt_conversion(stmt->command, nativeSQL);
	} else {
		stmt->type = SIMPLE_STATEMENT;
		strcpy(stmt->command, nativeSQL);
		stmt->p_values = NULL;
	}

	return stmt;
}

/**
 * PostgresResultSet_get - Retrieves the value of the designated column in the 
 * current row of this ResultSet object as a String. (this is an internal function)
 * @cx: JavaScript context
 * @argc: arguments' number
 * @vp: arguments' values
 *
 * Returns the number on success and JS_NULL on failure
 */
static inline JSBool
PostgresResultSet_get(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval this = JS_THIS(cx, vp);
	struct statement *stmt = (struct statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));
	JSBool ret = JS_TRUE;
	jsval rval = JSVAL_NULL;
	int column_index;

	if (stmt == NULL) {
		dlog(LOG_ALERT, "The statement property is not set\n");
		ret = JS_FALSE;
		goto out;
	}

	if (argc != 1) {
		dlog(LOG_WARNING, "Wrong number of arguments\n");
		ret = JS_FALSE;
		goto out;
	}

	if (!JSVAL_IS_INT(JS_ARGV(cx, vp)[0])) {
		dlog(LOG_WARNING, "The first parameter should be a number \
			which represents the position of the parameter or a string (column label)\n");
		ret = JS_FALSE;
		goto out;
	}

	if (JSVAL_IS_INT(JS_ARGV(cx, vp)[0])) {
		column_index = JSVAL_TO_INT(JS_ARGV(cx, vp)[0]);

		if (column_index < 1 || column_index > PQnfields(stmt->result)) {
			dlog(LOG_WARNING, "Column index out of bounds\n");
			ret = JS_FALSE;
			goto out;
		}

		column_index--;

		if (stmt->row_index < 0 || stmt->row_index >= PQntuples(stmt->result)) {
			dlog(LOG_WARNING, "Row index out of bounds\n");
			ret = JS_FALSE;
			goto out;
		}
	} else if (JSVAL_IS_STRING(JS_ARGV(cx, vp)[0])) {
		char *column_name = JSString_to_CString(cx, JS_ARGV(cx, vp)[0]);
		if (column_name == NULL) {
			ret = JS_FALSE;
			goto out;
		}

		column_index = PQfnumber(stmt->result, column_name);
		if (column_index == -1) {
			ret = JS_FALSE;
			dlog(LOG_WARNING, "The given name does not match any column.");
			goto out;
		}
	} else {
		dlog(LOG_WARNING, "[Wrong argument type]The first parameter should be a number \
		which represents the position of the parameter or a string (column label)\n");
		ret = JS_FALSE;
		goto out;
	}

	if (PQgetisnull(stmt->result, stmt->row_index, column_index)) {
		ret = JS_FALSE;
		//FIXME not sure if I should return a NULL/empty string
		goto out;
	}

	char *value = PQgetvalue(stmt->result, stmt->row_index, column_index);
	char *pEnd;
	long value_to_double = strtol(value, &pEnd, 10);

	//check if the value is a number
	if (pEnd == '\0' || pEnd != value) {
		rval = JS_NumberValue(value_to_double);
	} else {
		JSString *str = JS_NewStringCopyN(cx, value, strlen(value));	
		if (str)
			rval = STRING_TO_JSVAL(str);
	}

out:
	JS_SET_RVAL(cx, vp, rval);
	return ret;
}

/**
 * PostgresResultSet_getNumber - Retrieves the value of the designated column in the 
 * current row of this ResultSet object as a number.
 * @cx: JavaScript context
 * @argc: arguments' number
 * @vp: arguments' values
 *
 * Returns the number on success and JS_NULL on failure
 */
static JSBool PostgresResultSet_getNumber(JSContext *cx, unsigned argc, jsval *vp)
{
	if (!PostgresResultSet_get(cx, argc, vp))
		return JS_TRUE;

	if (!JSVAL_IS_NUMBER(*vp)) {
		dlog(LOG_WARNING, "The result value from that position is not a number\n");
		JS_SET_RVAL(cx, vp, JSVAL_NULL);
	}

	return JS_TRUE;
}

/**
 * PostgresResultSet_getString - Retrieves the value of the designated column in the 
 * current row of this ResultSet object as a String.
 * @cx: JavaScript context
 * @argc: arguments' number
 * @vp: arguments' values
 *
 * Returns the string on success and JS__NULL on failure
 */
static JSBool PostgresResultSet_getString(JSContext *cx, unsigned argc, jsval *vp)
{
	PostgresResultSet_get(cx, argc, vp);

	return JS_TRUE;
}

/**
 * PostgresResultSet_next - Moves the cursor froward one row from its current position.
 * @cx: JavaScript context
 * @argc: arguments' number
 * @vp: arguments' values
 *
 * Returns JS_TRUE on success and JS_FALSE on failure
 */
static JSBool PostgresResultSet_next(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval this = JS_THIS(cx, vp);
	struct statement *stmt = (struct statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));
	jsval rval = JSVAL_TRUE;

	if (stmt == NULL) {
		dlog(LOG_ALERT, "The statement property is not set\n");
		rval = JSVAL_FALSE;
		goto out;
	}

	if (argc != 0) {
		dlog(LOG_WARNING, "Wrong number of arguments\n");
		rval = JSVAL_FALSE;
		goto out;
	}

	if (stmt->row_index < 0 || stmt->row_index >= PQntuples(stmt->result) - 1) {
		dlog(LOG_INFO, "Row index out of bounds\n");
		rval = JSVAL_FALSE;
		goto out;
	}

	stmt->row_index += 1;

out:
	JS_SET_RVAL(cx, vp, rval);
	return JS_TRUE;
}

/**
 * PostgresResultSet_first - Moves the cursor to the first row in this ResultSet object.
 * @cx: JavaScript context
 * @argc: arguments' number
 * @vp: arguments' values
 *
 * Returns JS_TRUE on success and JS_FALSE on failure
 */
static JSBool PostgresResultSet_first(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval this = JS_THIS(cx, vp);
	struct statement *stmt = (struct statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));
	jsval rval = JSVAL_TRUE;

	if (stmt == NULL) {
		dlog(LOG_ALERT, "The statement property is not set\n");
		rval = JSVAL_FALSE;
		goto out;
	}

	if (argc != 0) {
		dlog(LOG_WARNING, "Wrong number of arguments\n");
		rval = JSVAL_FALSE;
		goto out;
	}

	stmt->row_index = 0;

out:
	JS_SET_RVAL(cx, vp, rval);
	return JS_TRUE;
}

/**
 * PostgresResultSet_last - Moves the cursor to the last row in this ResultSet object.
 * @cx: JavaScript context
 * @argc: arguments' number
 * @vp: arguments' values
 *
 * Returns JS_TRUE on success and JS_FALSE on failure
 */
static JSBool PostgresResultSet_last(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval this = JS_THIS(cx, vp);
	struct statement *stmt = (struct statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));
	jsval rval = JSVAL_TRUE;

	if (stmt == NULL) {
		dlog(LOG_ALERT, "The statement property is not set\n");
		rval = JSVAL_FALSE;
		goto out;
	}

	if (argc != 0) {
		dlog(LOG_WARNING, "Wrong number of arguments\n");
		rval = JSVAL_FALSE;
		goto out;
	}

	stmt->row_index = PQntuples(stmt->result) - 1;

out:
	JS_SET_RVAL(cx, vp, rval);
	return JS_TRUE;
}

static JSClass PostgresResultSet_class = {
	"PostgresResultSet",   //name of the class
	JSCLASS_HAS_PRIVATE,    //flags
	JS_PropertyStub,        //addProperty (default value JS_PropertyStrub)
	JS_PropertyStub,        //delProperty (default)
	JS_PropertyStub,        //getProperty (default)
	JS_StrictPropertyStub,  //setProperty (default value JS_StrictPropertyStub)
	JS_EnumerateStub,       //enumerate (default)
	JS_ResolveStub,         //resolve -lazy properties(default)
	JS_ConvertStub,         //conversion to primitive value (default)
	NULL, 					//optional members
};

static JSFunctionSpec PostgresResultSet_functions[] = {
	JS_FS("getNumber", PostgresResultSet_getNumber, 1, 0),
	JS_FS("getString", PostgresResultSet_getString, 1, 0),
	JS_FS("next", PostgresResultSet_next, 0, 0),
	JS_FS("first", PostgresResultSet_first, 0, 0),
	JS_FS("last", PostgresResultSet_last, 0, 0),
	JS_FS_END
};

/**
 * statement_get_result_set - Creates a ResultSet object
 * @cx: the JavaScript context
 * @stmt: pointer tot the structure which contains the result
 * @vp: return value
 *
 * Returns JS_TRUE on success and JS_FALSE on failure
 */
static JSBool
statement_get_result_set(JSContext *cx, struct statement *stmt, jsval *vp)
{
	JSBool ret = JS_TRUE;
	jsval rval = JSVAL_NULL;

	JSObject *obj = JS_NewObject(cx, &PostgresResultSet_class, NULL, NULL);
	if (obj == NULL) {
		ret = JS_FALSE;
		JS_ReportError(cx, "Failed to create a new object\n");
		dlog(LOG_ALERT, "Failed to create a new object\n");
		goto out;
	}

	JS_SetPrivate(obj, stmt);
	
	if (JS_DefineFunctions(cx, obj, PostgresResultSet_functions) == JS_FALSE) {
		ret = JS_FALSE;
		JS_ReportError(cx, "Failed to define functions for PostgresResultSet\n");
		dlog(LOG_ALERT, "Failed to define functions for PostgresResultSet\n");
		goto out;
	}

	rval = OBJECT_TO_JSVAL(obj);

out:
	JS_SET_RVAL(cx, vp, rval);
	return ret;
}

/**
 * Postgres_setStatement - Creates a new statement using the SQL command and saves
 * it in the Statement's objects properties. 
 * @cx: the JavaScript context
 * @argc: arguments number
 * @vp: return value
 * @obj: the instance of object on which the statement is set
 *
 * Returns JS_TRUE on success and JS_FALSE on failure
 */
static JSBool
Postgres_setStatement(JSContext *cx, unsigned argc, jsval *vp, jsval obj)
{
	JSBool ret = JS_TRUE;
	jsval nativeSQL_argv[] = {JS_ARGV(cx, vp)[0]};
	jsval nativeSQL_jsv;
	jsval conn;

	if (!JS_CallFunctionName(cx, JSVAL_TO_OBJECT(obj), "getConnection",
						argc, vp, &conn)) {
		ret = JS_FALSE;
		dlog(LOG_ALERT, "Failed to call getConnection\n");
		goto out;
	}

	if (JSVAL_IS_NULL(conn)) {
		ret = JS_FALSE;
		dlog(LOG_ALERT, "NULL Connection\n");
		goto out;
	}

	if (!JS_CallFunctionName(cx, JSVAL_TO_OBJECT(conn), "nativeSQL", 1,
							nativeSQL_argv, &nativeSQL_jsv)) {
		ret = JS_FALSE;
		dlog(LOG_ALERT, "Failed to call nativeSQL\n");
		goto out;
	}

	struct statement *stmt = generate_statement(cx, nativeSQL_jsv);
	if (stmt == NULL) {
		ret = JS_FALSE;
		dlog(LOG_ALERT, "Failed to generate the statement\n");
		goto out;
	}

	stmt->conn = (PGconn *)JS_GetPrivate(JSVAL_TO_OBJECT(conn));
	if (PQstatus(stmt->conn) != CONNECTION_OK) {
		dlog(LOG_ALERT, "Wrong connection status %s\n", PQerrorMessage(stmt->conn));
		free(stmt->command);
		free(stmt->p_values);
		free(stmt);
		ret = JS_FALSE;
		goto out;
	}

	JS_SetPrivate(JSVAL_TO_OBJECT(obj), stmt);
out:
	return ret;
}

/**
 * execute_statement - generic function for statement's execution
 * @cx: JavaScript context
 * @argc: arguments' number
 * @vp: arguments' values
 * @stmt: statement 
 *
 * Returns JS_TRUE on success and JS_FALSE on failure.
 */
static JSBool
execute_statement(JSContext *cx, unsigned argc, jsval *vp, struct statement *stmt)
{
	if (stmt == NULL) {
		dlog(LOG_WARNING, "NULL STATEMENT...\n");
		return JS_FALSE;
	}

	if (argc == 1 && stmt->type != SIMPLE_STATEMENT) {
		dlog(LOG_INFO, "Wrong statement type! You should use prepared statement\n");
		return JS_FALSE;
	} else if (argc == 0 && stmt->type != PREPARED_STATEMENT) {
		dlog(LOG_INFO, "Wrong statement type! You should use simple statement\n");
		return JS_FALSE;
	}

	stmt->result = PQexecParams(stmt->conn,
								stmt->command,
								stmt->p_len,	/* parameters' length */
								NULL,    		/* let the backend deduce param type */
								(const char **)stmt->p_values,
								NULL,			/* don't need param lengths since text */
								NULL,			/* default to all text params */
								TEXT_RESULT);	/* ask for text results */

	if (PQresultStatus(stmt->result) != PGRES_COMMAND_OK &&
		PQresultStatus(stmt->result) != PGRES_TUPLES_OK) {
		dlog(LOG_ERR, "%s command failed: %s\n",
			stmt->command, PQerrorMessage(stmt->conn));
		PQclear(stmt->result);
		//PQfinish(stmt->conn);
		return JS_FALSE;
	}

	return JS_TRUE;
}

/**
 * PostgresStatement_execute - Executes the given SQL statement, 
 * which may return multiple results. It returns false on error or
 * true on success.
 * @cx: JavaScript context
 * @argc: arguments' number
 * @vp: arguments' values
 *
 * Returns JS_TRUE on success and JS_FALSE on failure.
 */
static JSBool PostgresStatement_execute(JSContext *cx, unsigned argc, jsval *vp)
{	
	JSBool ret = JS_TRUE;
	/* if it is a simple statement set the SQL command*/
	jsval this = JS_THIS(cx, vp);

	if (argc == 1) {
		if (Postgres_setStatement(cx, argc, vp, this) == JS_FALSE) {
			ret = JS_FALSE;
			goto out;
		}
	}

	struct statement *stmt = 
		(struct statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));
	if (stmt == NULL) {
		ret = JS_FALSE;
		goto out;
	}

	ret = execute_statement(cx, argc, vp, stmt);
	if (stmt->type == SIMPLE_STATEMENT)
		clear_statement(stmt);

out:
	JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(ret));
	return JS_TRUE;
}

/**
 * PostgresStatement_executeQuery - Executes the given SQL statement, 
 * which returns a single result. On error returns NULL.
 * @cx: JavaScript context
 * @argc: arguments' number
 * @vp: arguments' values
 *
 * Returns JS_TRUE on success and JS_FALSE on failure and saves the ResultSet in vp.
 */
static JSBool
PostgresStatement_executeQuery(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval this = JS_THIS(cx, vp);

	/* if it is a simple statement set the SQL command*/
	if (argc == 1) {
		if (Postgres_setStatement(cx, argc, vp, this) == JS_FALSE) {
			goto out;
		}
	}

	struct statement *stmt = 
		(struct statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));

	if (stmt == NULL)
		goto out;

	if (execute_statement(cx, argc, vp, stmt) == JS_FALSE) {
		clear_statement(stmt);
		goto out;
	}

	if (statement_get_result_set(cx, stmt, &JS_RVAL(cx, vp)) == JS_FALSE)
		goto out;

	return JS_TRUE;

out:
	JS_SET_RVAL(cx, vp, JSVAL_NULL);
	return JS_TRUE;
}

/**
 * PostgresStatement_executeUpdate - Executes the given SQL statement, which may 
 * be an INSERT, UPDATE, or DELETE statement or an SQL statement that returns 
 * nothing, such as an SQL DDL statement. On error returns -1.
 * @cx: JavaScript context
 * @argc: arguments' number
 * @vp: arguments' values
 *
 * Returns JS_TRUE on success and JS_FALSE on failure and saves the affected
 * rows number in vp.
 */
static JSBool
PostgresStatement_executeUpdate(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval this = JS_THIS(cx, vp);
	jsval rval = JS_NumberValue(-1);

	/* if it is a simple statement set the SQL command*/
	if (argc == 1) {
		if (Postgres_setStatement(cx, argc, vp, this) == JS_FALSE) {
			goto out;
		}
	}

	struct statement *stmt = 
		(struct statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));
	if (stmt == NULL)
		goto out;

	if (execute_statement(cx, argc, vp, stmt) == JS_FALSE) {
		clear_statement(stmt);
		goto out;
	}

	rval = JS_NumberValue(atoi(PQcmdTuples(stmt->result)));	
	if(stmt->type == SIMPLE_STATEMENT)
		clear_statement(stmt);

out:
	JS_SET_RVAL(cx, vp, rval);
	return JS_TRUE;
}

/**
 * PostgresStatement_getConnection - Executes the given SQL statement, 
 * which may return multiple results.
 * @cx: JavaScript context
 * @argc: arguments' number
 * @vp: arguments' values
 *
 * Returns JS_TRUE on success and JS_FALSE on failure
 */
static JSBool
PostgresStatement_getConnection(JSContext *cx, unsigned argc, jsval *vp)
{
	return getConnection(cx, vp);
}

static JSFunctionSpec PostgresStatement_functions[] = {
	JS_FS("execute", PostgresStatement_execute, 1, 0),
	JS_FS("executeQuery", PostgresStatement_executeQuery, 1, 0),
	JS_FS("executeUpdate", PostgresStatement_executeUpdate, 1, 0),
	JS_FS("getConnection", PostgresStatement_getConnection, 1, 0),
	JS_FS_END
};

static JSClass PostgresStatement_class = {
	"PostgresStatement",   	//name of the class
	JSCLASS_HAS_PRIVATE,    //flags
	JS_PropertyStub,        //addProperty (default value JS_PropertyStrub)
	JS_PropertyStub,        //delProperty (default)
	JS_PropertyStub,        //getProperty (default)
	JS_StrictPropertyStub,  //setProperty (default value JS_StrictPropertyStub)
	JS_EnumerateStub,       //enumerate (default)
	JS_ResolveStub,         //resolve -lazy properties(default)
	JS_ConvertStub,         //conversion to primitive value (default)
	NULL, 					//optional members
};


/**
 * PostgresPreparedStatement_set - Generic set parameter function
 * which may return multiple results.
 * @cx: JavaScript context
 * @argc: arguments' number
 * @vp: arguments' values
 *
 * Returns JS_TRUE on success and JS_FALSE on failure.
 */
static inline JSBool
PostgresPreparedStatement_set(JSContext *cx, unsigned argc, jsval *vp, 
		struct statement **stmt)
{
	jsval this = JS_THIS(cx, vp);
	*stmt = (struct statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));
	JSBool ret = JS_TRUE;
	int pos;

	if (*stmt == NULL) {
		dlog(LOG_ALERT, "The statement property is not set\n");
		ret = JS_FALSE;
		goto out;
	}

	if (argc < 1) {
		dlog(LOG_WARNING, "Wrong number of arguments\n");
		ret = JS_FALSE;
		goto out;
	}

	if (!JSVAL_IS_INT(JS_ARGV(cx, vp)[0])) {
		dlog(LOG_WARNING, "The first parameter should be a number \
			which represents the position of the parameter\n");
		ret = JS_FALSE;
		goto out;
	}

	pos = JSVAL_TO_INT(JS_ARGV(cx, vp)[0]);

 	if ( pos > (*stmt)->p_len || pos < 1) {
		dlog(LOG_WARNING, "The position is incorrect!\n");
		ret = JS_FALSE;
		goto out;
	}

	pos--;

	if ((*stmt)->p_values[pos] != NULL) {
		dlog(LOG_INFO, "The element from position %d was already set : %s.\n", pos + 1, (*stmt)->p_values[pos] );
		free((*stmt)->p_values[pos]);
		(*stmt)->p_values[pos] = NULL;
	}

	//if the parameter is NULL, let the value to remain NULL
	if (argc < 2 || JSVAL_IS_NULL(JS_ARGV(cx, vp)[1])) {
		(*stmt)->p_values[pos] = NULL;
	} else {
		char *value = JSString_to_CString(cx, JS_ARGV(cx, vp)[1]);
		(*stmt)->p_values[pos] = malloc (strlen(value) * sizeof(char));
		if ((*stmt)->p_values[pos] == NULL) {
			dlog(LOG_WARNING, "Failed to allocate memory\n");
			ret = JS_FALSE;
			free(value);
			goto out;
		}
		strcpy((*stmt)->p_values[pos], value);
		free(value);
	}

out:
	JS_SET_RVAL(cx, vp, JSVAL_NULL);
	return ret;
}

/**
 * PostgresPreparedStatement_execute - Executes the given SQL statement, 
 * which may return multiple results.
 * @cx: JavaScript context
 * @argc: arguments' number
 * @vp: arguments' values
 *
 * Returns JS_TRUE on success and JS_FALSE on failure.
 */
static JSBool
PostgresPreparedStatement_setNumber(JSContext *cx, unsigned argc, jsval *vp)
{
	struct statement *stmt;
	jsval this = JS_THIS(cx, vp);
	jsval ret = JSVAL_TRUE;
	
	stmt = (struct statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));

	if (!JSVAL_IS_NUMBER(JS_ARGV(cx, vp)[1]) ||
		JSVAL_IS_NULL(JS_ARGV(cx, vp)[1])) {
		dlog(LOG_WARNING, "The value is not a number!\n");
		ret = JSVAL_FALSE;
		goto out;
	}

	ret = BOOLEAN_TO_JSVAL(PostgresPreparedStatement_set(cx, argc, vp, &stmt));

out:
	JS_SET_RVAL(cx, vp, ret);
	return JS_TRUE;
}

/**
 * PostgresPreparedStatement_execute - Executes the given SQL statement, 
 * which may return multiple results.
 * @cx: JavaScript context
 * @argc: arguments' number
 * @vp: arguments' values
 *
 * Returns JS_TRUE on success and JS_FALSE on failure.
 */
static JSBool
PostgresPreparedStatement_setString(JSContext *cx, unsigned argc, jsval *vp)
{
	struct statement *stmt;
	jsval this = JS_THIS(cx, vp);
	jsval ret = JSVAL_TRUE;
	
	stmt = (struct statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));

	if (!JSVAL_IS_STRING(JS_ARGV(cx, vp)[1]) || 
		JSVAL_IS_NULL(JS_ARGV(cx, vp)[1])) {
		dlog(LOG_WARNING, "The value is not a string!\n");
		ret = JSVAL_FALSE;
		goto out;
	}

	ret = BOOLEAN_TO_JSVAL(PostgresPreparedStatement_set(cx, argc, vp, &stmt));

out:
	JS_SET_RVAL(cx, vp, ret);
	return JS_TRUE;
}

/**
 * PostgresPreparedStatement_getGeneratedKeys - Returns the generated keys
 * @cx: JavaScript context
 * @argc: arguments' number
 * @vp: arguments' values
 */
static JSBool
PostgresPreparedStatement_getGeneratedKeys(JSContext *cx, unsigned argc, jsval *vp)
{
	/*
	 //OPTION 1
	 	step 1 : concatenate returning id to insert statements
	 		Example: "insert into yourtable(col1,col2,col3,...) values ($1,$2,$3,...) returning id"
		step 2 treat the PGresult as though I just done a select statement
			(I should use the name of the actual id column in the returning clause)
	
	 //OPTION 2
	 	select currval(sequence)
	*/
	return JS_TRUE;
}

/**
 * PostgresPreparedStatement_getResultSet - Returns the result set
 * @cx: JavaScript context
 * @argc: arguments' number
 * @vp: arguments' values
 */
static JSBool
PostgresPreparedStatement_getResultSet(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval this = JS_THIS(cx, vp);

	struct statement *stmt = 
		(struct statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));

	if (stmt == NULL)
		goto out;

	if (statement_get_result_set(cx, stmt, &JS_RVAL(cx, vp)) == JS_FALSE)
		goto out;

	return JS_TRUE;

out:
	JS_SET_RVAL(cx, vp, JSVAL_NULL);
	return JS_TRUE;
}

/**
 * PostgresPreparedStatement_getUpdateCount - Returns the number of updated rows
 * @cx: JavaScript context
 * @argc: arguments' number
 * @vp: arguments' values
 */
static JSBool
PostgresPreparedStatement_getUpdateCount(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval rval = JS_NumberValue(-1);
	jsval this = JS_THIS(cx, vp);

	struct statement *stmt = 
		(struct statement *)JS_GetPrivate(JSVAL_TO_OBJECT(this));

	if (stmt == NULL || stmt->result == NULL)
		goto out;

	rval = JS_NumberValue(atoi(PQcmdTuples(stmt->result)));	
	
out:
	JS_SET_RVAL(cx, vp, rval);
	return JS_TRUE;
}

static JSFunctionSpec PostgresPreparedStatement_functions[] = {
	JS_FS("execute", PostgresStatement_execute, 0, 0),
	JS_FS("executeQuery", PostgresStatement_executeQuery, 0, 0),
	JS_FS("executeUpdate", PostgresStatement_executeUpdate, 0, 0),
	JS_FS("getConnection", PostgresStatement_getConnection, 1, 0),
	JS_FS("getGeneratedKeys", PostgresPreparedStatement_getGeneratedKeys, 0, 0),
	JS_FS("getResultSet", PostgresPreparedStatement_getResultSet, 0, 0),
	JS_FS("getUpdateCount", PostgresPreparedStatement_getUpdateCount, 0, 0),
	JS_FS("setNumber", PostgresPreparedStatement_setNumber, 2, 0),
	JS_FS("setString", PostgresPreparedStatement_setString, 2, 0),
	JS_FS_END
};

static JSClass PostgresPreparedStatement_class = {
	"PostgresPreparedStatement",	//name of the class
	JSCLASS_HAS_PRIVATE,    		//flags
	JS_PropertyStub,        		//addProperty (default value JS_PropertyStrub)
	JS_PropertyStub,        		//delProperty (default)
	JS_PropertyStub,        		//getProperty (default)
	JS_StrictPropertyStub,  		//setProperty (default value JS_StrictPropertyStub)
	JS_EnumerateStub,       		//enumerate (default)
	JS_ResolveStub,         		//resolve -lazy properties(default)
	JS_ConvertStub,         		//conversion to primitive value (default)
	NULL, 							//optional members
};

/**
 * PostgresConnection_createStatement - Used to create a Statement object
 * @cx: JavaScript context
 * @argc: number of argumets
 * @vp: arguments array
 *
 * Returns JS_TRUE on success and JS_FALSE on failure.
 */
static JSBool 
PostgresConnection_createStatement(JSContext *cx, unsigned argc, jsval *vp)
{
	return createStatement(cx, vp, &PostgresStatement_class,
							PostgresStatement_functions);
}

/**
 * PostgresConnection_prepareStatement - Used to create a PreparedStatement 
 * object.
 * @cx: JavaScript context
 * @argc: number of argumets
 * @vp: arguments array
 *
 * Returns JS_TRUE on success and JS_FALSE on failure.
 */
static JSBool
PostgresConnection_prepareStatement(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval rval = JSVAL_NULL;

	JSObject *obj = JS_NewObject(cx, &PostgresPreparedStatement_class, NULL, NULL);
	if (obj == NULL) {
		JS_ReportError(cx, "Failed to create a new object\n");
		dlog(LOG_ALERT, "Failed to create a new object\n");
		goto out;
	}

	JS_DefineProperty(cx, obj, "connection", 
					JS_THIS(cx, vp), NULL, NULL, 
					JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);

	if (JS_DefineFunctions(cx, obj, PostgresPreparedStatement_functions) == JS_FALSE) {
		JS_ReportError(cx, "Failed to define functions in createStatement\n");
		dlog(LOG_ALERT, "Failed to define functions in createStatement\n");
		goto out;
	}

	if (Postgres_setStatement(cx, argc, vp, OBJECT_TO_JSVAL(obj)) == JS_FALSE)
		goto out;

	rval = OBJECT_TO_JSVAL(obj);

out:
	JS_SET_RVAL(cx, vp, rval);
	return JS_TRUE;
}

/**
 * PostgresConnection_nativeSQL - Used to convert SQL commands to native SQL
 * @cx: JavaScript context
 * @argc: number of argumets
 * @vp: arguments array
 *
 * Returns JS_TRUE on success and JS_FALSE on failure and saves the converted
 * value to vp.
 */
static JSBool
PostgresConnection_nativeSQL(JSContext *cx, unsigned argc, jsval *vp)
{
	return nativeSQL(cx, argc, vp);
}

static JSFunctionSpec PostgresConnection_functions[] = {
	JS_FS("createStatement", PostgresConnection_createStatement, 0, 0),
	JS_FS("prepareStatement", PostgresConnection_prepareStatement, 1, 0),
	JS_FS("nativeSQL", PostgresConnection_nativeSQL, 1, 0),
	JS_FS_END
};

static JSClass PostgresConnection_class = {
	"PostgresConnection",   //name of the class
	JSCLASS_HAS_PRIVATE,    //flags
	JS_PropertyStub,        //addProperty (default value JS_PropertyStrub)
	JS_PropertyStub,        //delProperty (default)
	JS_PropertyStub,        //getProperty (default)
	JS_StrictPropertyStub,  //setProperty (default value JS_StrictPropertyStub)
	JS_EnumerateStub,       //enumerate (default)
	JS_ResolveStub,         //resolve -lazy properties(default)
	JS_ConvertStub,         //conversion to primitive value (default)
	NULL, 					//optional members
};

/**
 * PostgresDriver_acceptsURL - checks if the URL is a valid one
 * @cx: JavaScript context
 * @argc: number of argumets
 * @vp: arguments array
 *
 * Returns JS_TRUE on success and JS_FALSE on failure
 */
static JSBool PostgresDriver_acceptsURL(JSContext * cx, unsigned argc, jsval * vp)
{
	char *url = JSString_to_CString(cx, JS_ARGV(cx, vp)[0]);
	if (url == NULL) {
		dlog(LOG_ALERT, "Failed to convert the URL value to JSString\n");
		goto out;
	}

	dlog(LOG_NOTICE, "Checking the PostgreSQL url: '%s'\t", url);

	if (strncmp(url, POSTGRES_URI, POSTGRES_URI_LEN)) {
		JS_SET_RVAL(cx, vp, JSVAL_FALSE);
		dlog(LOG_NOTICE, "BAD url. It should start with %s\n", POSTGRES_URI);
	} else {
		JS_SET_RVAL(cx, vp, JSVAL_TRUE);
		dlog(LOG_NOTICE, "GOOD url\n");
	}
	JS_free(cx, url);

	return JS_TRUE;

out:
	JS_SET_RVAL(cx, vp, JSVAL_FALSE);
	return JS_TRUE;
}

/**
 * PostgresDriver_connect - driver's connection function
 * @cx: JavaScript context
 * @argc: number of argumets
 * @vp: arguments array
 *
 * Returns JS_TRUE on success and JS_FALSE on failure
 */
static JSBool PostgresDriver_connect(JSContext * cx, unsigned argc, jsval * vp)
{
	JSBool ret = JS_TRUE;
	jsval rval = JSVAL_NULL;
	if (!argc)
		goto out;

	char *url = JSString_to_CString(cx, JS_ARGV(cx, vp)[0]);
	if (url == NULL) {
		JS_ReportError(cx,	"[PostgresDriver_connect] Failed to convert the URL "
					"value to JSString\n");
		dlog(LOG_ALERT, "Failed to convert the URL value to JSString\n");
		goto out_clean;
	}

	if (strncmp(url, POSTGRES_URI, POSTGRES_URI_LEN)) {
		JS_ReportWarning(cx, "Invalid url. It should start with %s\n",
			POSTGRES_URI);
		dlog(LOG_WARNING, "Invalid url. It should start with %s\n", POSTGRES_URI);
		goto out_clean;
	}

	char *host = url + POSTGRES_URI_LEN;
	unsigned int port = POSTGRES_DEFAULT_PORT;

	int sep_pos = strcspn(host, ":/");
	if (sep_pos == strlen(host))
		goto invalid_url;

	char *cursor = host + sep_pos;
	if (*cursor == ':') {
		char *port_str;
		*(cursor++) = '\0';
		port_str = cursor;
		int db_sep = strcspn(cursor, "/");

		if (db_sep == strlen(cursor))
		  goto invalid_url;

		cursor += db_sep;
		*(cursor++) = '\0';
		port = atoi(port_str);
	} else if (*cursor == '/') {
		*(cursor++) = '\0';
	} else {
		goto invalid_url;
	}

	//TODO Move the common code in a separated source
 	char *user = NULL, *passwd = NULL;
	if (argc > 1 && !JSVAL_IS_PRIMITIVE(JS_ARGV(cx, vp)[1]) &&
		!JSVAL_IS_NULL(JS_ARGV(cx, vp)[1])) {
		JSObject *info = JSVAL_TO_OBJECT(JS_ARGV(cx, vp)[1]);
		jsval user_jsv, passwd_jsv;

		if (JS_GetProperty(cx, info, "user", &user_jsv) && 
			!JSVAL_IS_NULL(user_jsv)) {
			JSString *user_str = JS_ValueToString(cx, user_jsv);
			user = JS_EncodeString(cx, user_str);
		}

		if (JS_GetProperty(cx, info, "password", &passwd_jsv) &&
			!JSVAL_IS_NULL(passwd_jsv)) {
			JSString *passwd_str = JS_ValueToString(cx, passwd_jsv);
			passwd = JS_EncodeString(cx, passwd_str);
		}
	}

	char postgres_connector[POSTGRES_CONNECTOR_LEN];
	sprintf(postgres_connector, "dbname=%s hostaddr=%s port=%d",
			cursor, host, port);

	if (user != NULL) {
		strcat(postgres_connector, " user=");
		strcat(postgres_connector, user);
		JS_free(cx, user);
	}

	if (passwd != NULL) {
		strcat(postgres_connector, " password=");
		strcat(postgres_connector, passwd);
		JS_free(cx, passwd);
	}

	PGconn *connection;
	connection = PQconnectdb(postgres_connector);
	if (PQstatus(connection) != CONNECTION_OK) {
		ret = JS_FALSE;
		JS_ReportError(cx, "Connection failed %s", PQerrorMessage(connection));
		dlog(LOG_ERR, "Connection failed %s", PQerrorMessage(connection));
		PQfinish(connection);
		goto out_clean;
	}

	JSObject *retobj = JS_NewObject(cx, &PostgresConnection_class, NULL, NULL);
	if (retobj == NULL) {
		ret = JS_FALSE;
		JS_ReportError(cx, "Failed to create a new object\n");
		dlog(LOG_ALERT, "Failed to create a new object\n");
		goto out_clean;
	}

	JS_SetPrivate(retobj, connection);
	if (JS_DefineFunctions(cx, retobj, PostgresConnection_functions) == JS_FALSE) {
		ret = JS_FALSE;
		JS_ReportError(cx, "Failed to define functions for the PostgresConnection "
						"class\n");
		dlog(LOG_ALERT, "Failed to define functions for the PostgresConnection class\n");
		goto out_clean;
	}

	rval = OBJECT_TO_JSVAL(retobj);
	goto out_clean;

invalid_url:
	JS_ReportWarning(cx, "Invalid url. The database should be separated by '/'\n");
	dlog(LOG_WARNING, "Invalid url. The database should be separated by '/'\n");
out_clean:
	JS_free(cx, url);
out:
	JS_SET_RVAL(cx, vp, rval);
	return ret;
}

static JSFunctionSpec PostgresDriver_functions[] = {
	JS_FS("acceptsURL", PostgresDriver_acceptsURL, 1, 0),
	JS_FS("connect", PostgresDriver_connect, 2, 0)
};

/**
 * JS_PostgresConstructAndRegister - Constructs a JavaScript object which will 
 * contain PostgreSql driver functions and register it.
 * @cx: JavaScript context
 * @global: global JavaScript object
 *
 * Returns JS_TRUE on success and JS_FALSE on failure
 */
JSBool JS_PostgresConstructAndRegister(JSContext * cx, JSObject * global)
{
	JSObject *obj = JS_NewObject(cx, NULL, NULL, NULL);

	if (!JS_DefineFunctions(cx, obj, PostgresDriver_functions))
		return JS_FALSE;

	return JS_SqlRegisterDriver(cx, global, obj);
}