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
};

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

	ret = execute_statement(cx, argc, vp, stmt);
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

	if (execute_statement(cx, argc, vp, stmt) == JS_FALSE) {
		clear_statement(stmt);
		goto out;
	}

	rval = JS_NumberValue(atoi(PQcmdTuples(stmt->result)));	
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
	return JS_FALSE;
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
		printf("here");
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