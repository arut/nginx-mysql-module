/*************************************************************************
  > File Name: ngx_http_mysql_output.h
  > Author: DenoFiend
  > Mail: denofiend@gmail.com
  > Created Time: 2012年12月26日 星期三 16时46分13秒
 ************************************************************************/


#ifndef _NGX_MYSQL_OUTPUT_H_
#define _NGX_MYSQL_OUTPUT_H_

#include <ngx_core.h>
#include <ngx_http.h>
#include <mysql.h>
#include "resty_dbd_stream.h"
#include "ngx_http_mysql_module.h"


//ngx_int_t        ngx_mysql_output_value(ngx_http_request_t *, MYSQL_RES *);
//ngx_int_t        ngx_mysql_output_text(ngx_http_request_t *, MYSQL_RES *);
ngx_int_t        ngx_mysql_output_rds(ngx_http_request_t *, MYSQL_RES *);
ngx_chain_t     *ngx_mysql_render_rds_header(ngx_http_request_t *,	ngx_pool_t *, ngx_int_t, ngx_int_t, const char* errstr, ngx_int_t, ngx_int_t);
ngx_chain_t     *ngx_mysql_render_rds_columns(ngx_http_request_t *, ngx_pool_t *,  MYSQL_FIELD *, ngx_int_t);
ngx_chain_t     *ngx_mysql_render_rds_row(ngx_http_request_t *, ngx_pool_t *, MYSQL_ROW, ngx_uint_t *, ngx_int_t, ngx_int_t, ngx_int_t);
ngx_chain_t     *ngx_mysql_render_rds_row_terminator(ngx_http_request_t *, ngx_pool_t *);
ngx_int_t        ngx_mysql_output_chain(ngx_http_request_t *, ngx_chain_t *);
rds_col_type_t   ngx_mysql_rds_col_type(enum enum_field_types);

#endif /* _NGX_MYSQL_OUTPUT_H_ */

