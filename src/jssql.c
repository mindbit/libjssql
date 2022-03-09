/* SPDX-License-Identifier: MIT */

#include <jsmisc.h>
#include "config.h"
#include "jssql.h"

static int DriverManager_getConnection(duk_context *ctx)
{
	duk_size_t i, len;
	int argc = duk_get_top(ctx);
	const char *url = duk_safe_to_string(ctx, 0);
	const char *user, *password;

	if (argc == 1)
		duk_push_object(ctx);

	if (argc == 2) {
		if (!duk_is_object(ctx, 1))
			return DUK_RET_ERROR;

		duk_push_string(ctx, "user");
		if (!duk_has_prop(ctx, 1))
			return DUK_RET_ERROR;

		duk_push_string(ctx, "password");
		if (!duk_has_prop(ctx, 1))
			return DUK_RET_ERROR;
	}

	if (argc == 3) {
		user = duk_safe_to_string(ctx, 1);
		password = duk_safe_to_string(ctx, 2);

		/* Create the info object */
		duk_push_object(ctx);

		duk_push_string(ctx, "user");
		duk_push_string(ctx, user);
		duk_put_prop(ctx, -3);

		duk_push_string(ctx, "password");
		duk_push_string(ctx, password);
		duk_put_prop(ctx, -3);
	}

	if (argc > 3)
		return DUK_RET_ERROR;

	/* Push DriverManager object onto the stack */
	duk_push_this(ctx);

	/* get drivers property to the stack */
	duk_get_prop_string(ctx, -1, "drivers");

	len = duk_get_length(ctx, -1);

	for (i = 0; i < len; i++) {
		duk_get_prop_index(ctx, -1, i);
		duk_get_prop_string(ctx, -1, "connect");

		duk_dup(ctx, -2);
		duk_push_string(ctx, url);

		duk_dup(ctx, -7);

		duk_call_method(ctx, 2);

		/* If the connect function doesn't return null and there are no errors,
		the return value will be on top of the stack */
		if (!duk_is_null(ctx, -1))
			return 1;

		/* Pop retval and index */
		duk_pop_2(ctx);
	}

	/* If none of the drivers were successful in creating a connection,
	error should be thrown. */
	return DUK_RET_ERROR;
}

static int DriverManager_getDriver(duk_context *ctx)
{
	duk_size_t i, len;
	const char *url = duk_safe_to_string(ctx, 0);

	duk_push_this(ctx);

	duk_get_prop_string(ctx, -1, "drivers");
	len = duk_get_length(ctx, -1);

	for (i = 0; i < len; i++) {
		duk_get_prop_index(ctx, -1, i);
		duk_get_prop_string(ctx, -1, "acceptsURL");
		duk_dup(ctx, -2);
		duk_push_string(ctx, url);
		duk_call_method(ctx, 1);

		if (duk_get_boolean(ctx, -1)) {
			/* Pop retval from acceptsURL so that the driver
			remains on top of the stack */
			duk_pop(ctx);
			return 1;
		}

		duk_pop_2(ctx);
	}

	return DUK_RET_ERROR;
}

static int DriverManager_registerDriver(duk_context *ctx)
{
	duk_push_this(ctx);

	if (!duk_get_prop_string(ctx, -1, "drivers")) {
		duk_pop(ctx);
		return DUK_RET_ERROR;
	}

	duk_dup(ctx, 0);
	js_append_array_element(ctx, -2);

	return 0;
}

static duk_function_list_entry DriverManager_functions[] = {
	{"getConnection",	DriverManager_getConnection,	DUK_VARARGS},
	{"getDriver",		DriverManager_getDriver,	1},
	{"registerDriver",	DriverManager_registerDriver,	1},
	{NULL,			NULL,				0}
};

static duk_number_list_entry Statement_constants[] = {
	{"NO_GENERATED_KEYS",		0.0},
	{"RETURN_GENERATED_KEYS",	1.0},
	{NULL,				0.0}
};

duk_bool_t js_sql_init(duk_context *ctx)
{
	/* Create the global statement object */
	duk_push_object(ctx);
	duk_put_number_list(ctx, -1, Statement_constants);
	duk_put_global_string(ctx, "Statement");

	/* Create DriverManager object */
	duk_push_object(ctx);

	duk_push_array(ctx);
	duk_put_prop_string(ctx, -2, "drivers");

	duk_put_function_list(ctx, -1, DriverManager_functions);

	duk_put_global_string(ctx, "DriverManager");
	return 1;
}
