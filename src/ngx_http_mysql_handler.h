/*************************************************************************
        > File Name: ngx_http_mysql_handler.h
      > Author: DenoFiend
      > Mail: denofiend@gmail.com
      > Created Time: 2012年12月27日 星期四 10时57分17秒
 ************************************************************************/

#ifndef __NGX_HTTP_MYSQL_HANDLER_H__
#define __NGX_HTTP_MYSQL_HANDLER_H__

#include <ngx_core.h>
#include <ngx_http.h>

ngx_int_t ngx_http_mysql_process_response(ngx_http_request_t *r);
ngx_int_t ngx_http_mysql_transaction_handler(ngx_http_request_t *r);
ngx_int_t ngx_http_mysql_handler(ngx_http_request_t *r);
ngx_int_t ngx_http_mysql_subrequest_handler(ngx_http_request_t *r);


#endif/*__NGX_HTTP_MYSQL_HANDLER_H__*/

