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
	NULL, //optional members
};

static JSFunctionSpec PostgresConnection_functions[] = {
	JS_FS_END
};

/**
 * PostgresDriver_acceptsURL - check if the URL is a valid one
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
 * Returns JS_TRUE on succeed and JS_FALSE on failure
 */
static JSBool PostgresDriver_connect(JSContext * cx, unsigned argc, jsval * vp)
{
	JSBool ret = JS_FALSE;
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

	ret = JS_TRUE;

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
	dlog(LOG_INFO, "[INFO] %s %d %s\n", host, port, cursor);
	//TODO Move the common code in a separated source
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
 * JS_PostgresConstructAndRegister - Construct an JavaScript object which will 
 * contain PostgreSql driver functions and register it.
 * @cx: JavaScript context
 * @global: global JavaScript object
 *
 * Returns JS_TRUE on succeed and JS_FALSE on failure
 */
JSBool JS_PostgresConstructAndRegister(JSContext * cx, JSObject * global)
{
	JSObject *obj = JS_NewObject(cx, NULL, NULL, NULL);

	if (!JS_DefineFunctions(cx, obj, PostgresDriver_functions))
		return JS_FALSE;

	return JS_SqlRegisterDriver(cx, global, obj);
}
