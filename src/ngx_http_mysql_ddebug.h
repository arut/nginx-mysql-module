/*************************************************************************
  > File Name: ngx_http_mysql_debug.h
  > Author: DenoFiend
  > Mail: denofiend@gmail.com
  > Created Time: 2012年12月26日 星期三 16时54分48秒
 ************************************************************************/

#ifndef _NGX_MYSQL_DDEBUG_H_
#define _NGX_MYSQL_DDEBUG_H_

#include <ngx_core.h>

#if defined(DDEBUG) && (DDEBUG)

#   if (NGX_HAVE_VARIADIC_MACROS)

#       define dd(...) fprintf(stderr, "postgres *** %s: ", __func__); \
	fprintf(stderr, __VA_ARGS__); \
fprintf(stderr, " *** %s line %d.\n", __FILE__, __LINE__)

#   else

#include <stdarg.h>
#include <stdio.h>

#include <stdarg.h>

	static void
dd(const char * fmt, ...)
{
	/* TODO */
}

#    endif

#else

#   if (NGX_HAVE_VARIADIC_MACROS)

#       define dd(...)

#   else

#include <stdarg.h>

	static void
dd(const char * fmt, ...)
{
}

#   endif

#endif

#endif /* _NGX_MYSQL_DDEBUG_H_ */

