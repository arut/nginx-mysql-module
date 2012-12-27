/*************************************************************************
  > File Name: ngx_http_mysql_module.h
  > Author: DenoFiend
  > Mail: denofiend@gmail.com
  > Created Time: 2012年12月26日 星期三 16时22分00秒
 ************************************************************************/

#ifndef _NGX_MYSQL_MODULE_H_
#define _NGX_MYSQL_MODULE_H_

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <mysql.h>
#include <ngx_http_mtask_module.h>


#define NGXCSTR(s) ((s).data ? strndupa((char*)(s).data, (s).len) : NULL)


extern ngx_module_t ngx_http_mysql_module;

typedef ngx_int_t (*ngx_mysql_output_handler_pt)
	    (ngx_http_request_t *, MYSQL_RES *);

typedef struct ngx_http_mysql_trans_loc_conf_s{
	ngx_array_t *query_lengths;
	ngx_array_t *query_values;
	ngx_str_t sql;
	ngx_uint_t len;
}ngx_http_mysql_trans_loc_conf_t;

struct ngx_http_mysql_loc_conf_s {

	ngx_array_t *query_lengths;
	ngx_array_t *query_values;

	ngx_array_t *subreq_lengths;
	ngx_array_t *subreq_values;

	/* for transaction*/
	ngx_http_mysql_trans_loc_conf_t *transaction_sqls;

	/* fro output */
	ngx_mysql_output_handler_pt output_handler;
};

typedef struct ngx_http_mysql_loc_conf_s ngx_http_mysql_loc_conf_t;

struct ngx_http_mysql_node_s {

	MYSQL mysql;

	unsigned ready:1;

	struct ngx_http_mysql_node_s *next;
};

typedef struct ngx_http_mysql_node_s ngx_http_mysql_node_t;

struct ngx_http_mysql_srv_conf_s {

	ngx_str_t host;
	ngx_int_t port;
	ngx_str_t user;
	ngx_str_t password;
	ngx_str_t database;
	ngx_str_t charset;

	ngx_int_t max_conn;
	ngx_flag_t multi;
	ngx_flag_t mysql_auto_commit;

	ngx_http_mysql_node_t *nodes;
	ngx_http_mysql_node_t *free_node;
};

typedef struct ngx_http_mysql_srv_conf_s ngx_http_mysql_srv_conf_t;

struct ngx_http_mysql_ctx_s {

	MYSQL *current;

	ngx_chain_t *subreq_out;

	/* for rd_json output_handler */
	ngx_chain_t *response;
	ngx_int_t var_cols;
	ngx_int_t var_rows;
	ngx_int_t var_affected;
	ngx_str_t var_query;
	ngx_array_t *variables;
	ngx_int_t status;
	ngx_int_t errcode;
	ngx_int_t insert_id;
	const char* errstr;
	//ngx_st_t errstr;
};

typedef struct ngx_http_mysql_ctx_s ngx_http_mysql_ctx_t;

void* ngx_http_mysql_create_srv_conf(ngx_conf_t *cf);
char* ngx_http_mysql_merge_srv_conf(ngx_conf_t *cf, void *parent, void *child);

void* ngx_http_mysql_create_loc_conf(ngx_conf_t *cf);
char* ngx_http_mysql_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);

char* ngx_http_mysql_query(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
char* ngx_http_mysql_transaction(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
char* ngx_http_mysql_subrequest(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
char* ngx_http_mysql_escape(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

ngx_int_t ngx_http_mysql_init(ngx_conf_t *cf);

#endif /* _NGX_MYSQL_MODULE_H_ */

