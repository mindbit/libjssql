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

static int run_test(char *source, duk_context *ctx)
{
	int fd;
	void *buf;
	off_t len;

	fd = open(source, O_RDONLY, 0);
	if (fd < 0)
		return 1;

	len = lseek(fd, 0, SEEK_END);
	buf = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);

	duk_push_lstring(ctx, buf, len);
	duk_push_string(ctx, source);
	if (duk_pcompile(ctx, 0))
		return 1;

	if (duk_pcall(ctx, 0)) {
		if (duk_is_error(ctx, -1)) {
			duk_get_prop_string(ctx, -1, "stack");
			printf("error: %s\n", duk_safe_to_string(ctx, -1));
		}
		return 1;
	}

	munmap(buf, len);
	close(fd);

	return 0;
}

int main(int argc, const char *argv[])
{

	duk_context *ctx = NULL;

	ctx = duk_create_heap(NULL, NULL, NULL, NULL, NULL);
	if (!ctx)
		return 1;

	duk_push_global_object(ctx);

	if (!js_misc_init(ctx, -1))
		return 1;

	if (!js_sql_init(ctx))
		return 1;

	int ret_mysql = 0;


#ifdef HAVE_MYSQL
	printf ("[Running mysql tests]\n");
	if(!js_mysql_construct_and_register(ctx))
		return 1;
	ret_mysql = run_test("test_mysql.js", ctx);
	printf("----------------------------------------------------\n");
	printf("\n%s: test_mysql\n", (ret_mysql == 0)? "PASS" : "FAIL");
#endif

#if 0

#ifdef HAVE_POSTGRESQL
	printf ("\n[Running postgresql tests]\n");
	JS_PgsqlConstructAndRegister(cx, global);
	ret_postgres = run_test("test_pgsql.js", cx, global);
	printf("----------------------------------------------------\n");
	printf("\n%s: test_postgres\n\n", (ret_postgres == 0)? "PASS" : "FAIL");
#endif
}

#endif

	duk_pop(ctx);

	duk_destroy_heap(ctx);

	return 0;
}
