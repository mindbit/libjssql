#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

#include "js_sql.h"

/* The class of the global object. */
static JSClass global_class = {
	"global", JSCLASS_GLOBAL_FLAGS,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
	JSCLASS_NO_OPTIONAL_MEMBERS
};

/* The error reporter callback. */
void reportError(JSContext *cx, const char *message, JSErrorReport *report)
{
	fprintf(stderr, "%s:%u:%s\n",
			report->filename ? report->filename : "<no filename=\"filename\">",
			(unsigned int) report->lineno,
			message);
}

int main(int argc, const char *argv[])
{
	/* JSAPI variables. */
	JSRuntime *rt;
	JSContext *cx;
	JSObject  *global;

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
	JS_SetOptions(cx, JSOPTION_VAROBJFIX | JSOPTION_JIT | JSOPTION_METHODJIT);
	JS_SetVersion(cx, JSVERSION_LATEST);
	JS_SetErrorReporter(cx, reportError);

	/*
	 * Create the global object in a new compartment.
	 * You always need a global object per context.
	 */
	global = JS_NewCompartmentAndGlobalObject(cx, &global_class, NULL);
	if (global == NULL)
		return 1;

	/*
	 * Populate the global object with the standard JavaScript
	 * function and object classes, such as Object, Array, Date.
	 */
	if (!JS_InitStandardClasses(cx, global))
		return 1;

	if (!JS_SqlInit(cx, global))
		return 1;

	int fd;
	void *buf;
	off_t len;

	fd = open("test.js", O_RDONLY, 0);
	len = lseek(fd, 0, SEEK_END);
	buf = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);

	/* Your application code here. This may include JSAPI calls
	 * to create your own custom JavaScript objects and to run scripts.
	 *
	 * The following example code creates a literal JavaScript script,
	 * evaluates it, and prints the result to stdout.
	 *
	 * Errors are conventionally saved in a JSBool variable named ok.
	 */
	jsval rval;
	uintN lineno = 0;

	JS_EvaluateScript(cx, global, buf, len, "noname", lineno, &rval);
	munmap(buf, len);
	/* End of your application code */

	/* Clean things up and shut down SpiderMonkey. */
	JS_DestroyContext(cx);
	JS_DestroyRuntime(rt);
	JS_ShutDown();
	return 0;
}
