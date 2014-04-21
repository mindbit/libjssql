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

#ifndef _JS_MYSQL_H
#define _JS_MYSQL_H

#include <libpq-fe.h>
#include "js_sql.h"
#include "js_utils.h"

#define POSTGRES_URI			"postgresql://"
#define POSTGRES_URI_LEN        13
#define POSTGRES_DEFAULT_PORT   5432
#define POSTGRES_CONNECTOR_LEN	256

JSBool JS_PostgresConstructAndRegister(JSContext *cx, JSObject *global);

#endif
