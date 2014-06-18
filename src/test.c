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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

#include "js_mysql.h"
#include "js_postgres.h"
#include "jssql_config.h"

/* The class of the global object. */
static JSClass global_class = {
	"global", JSCLASS_GLOBAL_FLAGS,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, NULL,
	JSCLASS_NO_OPTIONAL_MEMBERS
};

/* The error reporter callback. */
void reportError(JSContext *cx, const char *message, JSErrorReport *report)
{
	fprintf(stderr, "ERROR [%s:%u] %s\n",
			report->filename ? report->filename : "<no filename>",
			(unsigned int) report->lineno, message);
}

static JSBool gettimeoftheday(JSContext *cx, unsigned argc, jsval *vp)
{
	struct timeval t;
    double elapsedTime;
    jsval rval;

    // start timer
    gettimeofday(&t, NULL);
    elapsedTime = t.tv_sec * 1000000 + t.tv_usec;

	rval = JS_NumberValue(elapsedTime);;	

	JS_SET_RVAL(cx, vp, rval);
	return JS_TRUE;
}

static JSBool js_print(JSContext *cx, unsigned argc, jsval *vp)
{
	unsigned i;

	for (i = 0; i < argc; i++) {
		JSString *str = JS_ValueToString(cx, JS_ARGV(cx, vp)[i]);
		// FIXME check return value
		// FIXME root str (protect from GC) -> https://developer.mozilla.org/en-US/docs/SpiderMonkey/JSAPI_Reference/JS_ValueToString

		char *c_str = JS_EncodeString(cx, str);
		fputs(c_str, stdout);
		JS_free(cx, c_str);
	}

	return JS_TRUE;
}

static JSBool js_println(JSContext *cx, unsigned argc, jsval *vp)
{
	js_print(cx, argc, vp);
	putc('\n', stdout);
	return JS_TRUE;
}

static void js_dump_value(JSContext *cx, jsval v)
{
	char *c_str;
	JSString *js_str;

	switch (JS_TypeOfValue(cx, v)) {
	case JSTYPE_VOID:
		printf("VOID\n");
		break;
	case JSTYPE_OBJECT:
		if (JSVAL_IS_NULL(v)) {
			printf("NULL\n");
			break;
		}
		if (JS_IsArrayObject(cx, JSVAL_TO_OBJECT(v))) {
			printf("ARRAY\n");
			break;
		}
		printf("OBJECT {\n");
		printf("}\n");
		break;
	case JSTYPE_FUNCTION:
		printf("FUNCTION\n");
		break;
	case JSTYPE_STRING:
		c_str = JS_EncodeString(cx, JSVAL_TO_STRING(v));
		printf("STRING(%zu) \"%s\"\n", strlen(c_str), c_str);
		JS_free(cx, c_str);
		break;
	case JSTYPE_NUMBER:
		js_str = JS_ValueToString(cx, v);
		c_str = JS_EncodeString(cx, js_str);
		printf("NUMBER(%s)\n", c_str);
		JS_free(cx, c_str);
		break;
	case JSTYPE_BOOLEAN:
		js_str = JS_ValueToString(cx, v);
		c_str = JS_EncodeString(cx, js_str);
		printf("BOOLEAN(%s)\n", c_str);
		JS_free(cx, c_str);
		break;
	default:
		printf("FIXME\n");
		break;
	}
}

static JSBool js_dump(JSContext *cx, unsigned argc, jsval *vp)
{
	unsigned i;

	for (i = 0; i < argc; i++)
		js_dump_value(cx, JS_ARGV(cx, vp)[i]);

	return JS_TRUE;
}

static int run_test(char *source, JSContext *cx, JSObject *global) {
	int fd;
	void *buf;
	off_t len;

	fd = open(source, O_RDONLY, 0);
	if (fd < 0)
		return -1;
	
	len = lseek(fd, 0, SEEK_END);
	buf = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);

	jsval rval;
	uint lineno = 0;

	JS_EvaluateScript(cx, global, buf, len, "noname", lineno, &rval);
	
	munmap(buf, len);
	close(fd);
	if (JSVAL_IS_INT(rval))
		return JSVAL_TO_INT(rval);

	return -1;
}

int main(int argc, const char *argv[])
{
	/* JSAPI variables. */
	JSRuntime *rt;
	JSContext *cx;
	JSObject  *global;

	JS_SetCStringsAreUTF8();

	/* Create a JS runtime. You always need at least one runtime per process. */
	rt = JS_NewRuntime(8 * 1024 * 1024);
	if (rt == NULL)
		return 1;

	/*
	 * Create a context. You always need a context per thread.
	 * Note that this program is not multi-threaded.
	 */
	cx = JS_NewContext(rt, 8192);
	if (cx == NULL)
		return 1;
	JS_SetOptions(cx, JSOPTION_VAROBJFIX | JSOPTION_METHODJIT);
	JS_SetVersion(cx, JSVERSION_LATEST);
	JS_SetErrorReporter(cx, reportError);

	/*
	 * Create the global object in a new compartment.
	 * You always need a global object per context.
	 */
	JS_BeginRequest(cx); // needed by js-17.0.0 DEBUG
	global = JS_NewGlobalObject(cx, &global_class, NULL); // works with js-17.0.0
	//global = JS_NewGlobalObject(cx, &global_class); // compiles with js-1.8.5, but segfaults
	//global = JS_NewCompartmentAndGlobalObject(cx, &global_class, NULL); // works with js-1.8.5
	if (global == NULL)
		return 1;

	/*
	 * Populate the global object with the standard JavaScript
	 * function and object classes, such as Object, Array, Date.
	 */
	if (!JS_InitStandardClasses(cx, global))
		return 1;

	JS_DefineFunction(cx, global, "dump", js_dump, 0, 0);
	JS_DefineFunction(cx, global, "print", js_print, 0, 0);
	JS_DefineFunction(cx, global, "println", js_println, 0, 0);
	JS_DefineFunction(cx, global, "gettimeoftheday", gettimeoftheday, 0, 0);

	if (!JS_SqlInit(cx, global))
		return 1;

	int ret_mysql = 0, ret_postgres = 0;

#ifdef HAVE_MYSQL_MYSQL_H
	printf ("[Running mysql tests]\n");
	JS_MysqlConstructAndRegister(cx, global);
	ret_mysql = run_test("test_mysql.js", cx, global);
	printf("----------------------------------------------------\n");
	printf("\n%s: test_mysql\n", (ret_mysql == 0)? "PASS" : "FAIL");
#endif

#ifdef HAVE_POSTGRESQL_LIBPQ_FE_H
	printf ("\n[Running postgresql tests]\n");
	JS_PostgresConstructAndRegister(cx, global);	
	ret_postgres = run_test("test_postgres.js", cx, global);
	printf("----------------------------------------------------\n");
	printf("\n%s: test_postgres\n\n", (ret_postgres == 0)? "PASS" : "FAIL");
#endif
	JS_EndRequest(cx); // needed by js-17.0.0 DEBUG

	/* End of your application code */

	/* Clean things up and shut down SpiderMonkey. */
	JS_DestroyContext(cx);
	JS_DestroyRuntime(rt);
	JS_ShutDown();

	return ret_mysql && ret_postgres;

}
