/* SPDX-License-Identifier: MIT */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <jsmisc.h>

#include "config.h"
#include "jssql.h"
#include "jsmysql.h"
#include "jspgsql.h"

/* The class of the global object. */
static JSClass global_class = {
	"global", JSCLASS_GLOBAL_FLAGS,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, NULL,
	JSCLASS_NO_OPTIONAL_MEMBERS
};

static int run_test(char *source, JSContext *cx, JSObject *global)
{
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

	JS_EvaluateScript(cx, global, buf, len, source, lineno, &rval);

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
	JS_MiscSetErrorReporter(cx);

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

	JS_MiscInit(cx, global);

	if (!JS_SqlInit(cx, global))
		return 1;

	int ret_mysql = 0, ret_postgres = 0;

#ifdef HAVE_MYSQL
	printf ("[Running mysql tests]\n");
	JS_MysqlConstructAndRegister(cx, global);
	ret_mysql = run_test("test_mysql.js", cx, global);
	printf("----------------------------------------------------\n");
	printf("\n%s: test_mysql\n", (ret_mysql == 0)? "PASS" : "FAIL");
#endif

#ifdef HAVE_POSTGRESQL
	printf ("\n[Running postgresql tests]\n");
	JS_PgsqlConstructAndRegister(cx, global);
	ret_postgres = run_test("test_pgsql.js", cx, global);
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
