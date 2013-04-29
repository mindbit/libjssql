#ifndef _JS_SQL_H
#define _JS_SQL_H

#include <jsapi.h>

JSBool JS_SqlInit(JSContext *cx, JSObject *global);
JSBool JS_SqlRegisterDriver(JSContext *cx, JSObject *global, JSObject *driver);

#endif
