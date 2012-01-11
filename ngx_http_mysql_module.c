/******************************************************************************
Copyright (c) 2012, Roman Arutyunyan (arut@qip.ru)
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice, 
      this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ''AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
OF SUCH DAMAGE.
*******************************************************************************/

/*
     NGINX MySQL module

   * Makes use of libmysqlclient.so
   * Completely asynchronous
   * Uses nginx-mtask-module 

*/

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <mysql.h>
#include <ngx_http_mtask_module.h>

static void* ngx_http_mysql_create_loc_conf(ngx_conf_t *cf);
static char* ngx_http_mysql_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static char* ngx_http_mysql_query(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

struct ngx_http_mysql_loc_conf_s {

	ngx_str_t host;
	ngx_int_t port;
	ngx_str_t user;
	ngx_str_t password;
	ngx_str_t database;

	ngx_array_t *query_lengths;
	ngx_array_t *query_values;
};

typedef struct ngx_http_mysql_loc_conf_s ngx_http_mysql_loc_conf_t;

static ngx_command_t ngx_http_mysql_commands[] = {

	{	ngx_string("mysql_host"),
		NGX_HTTP_LOC_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(ngx_http_mysql_loc_conf_t, host),
		NULL },

	{	ngx_string("mysql_port"),
		NGX_HTTP_LOC_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_num_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(ngx_http_mysql_loc_conf_t, port),
		NULL },

	{	ngx_string("mysql_user"),
		NGX_HTTP_LOC_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(ngx_http_mysql_loc_conf_t, user),
		NULL },

	{	ngx_string("mysql_password"),
		NGX_HTTP_LOC_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(ngx_http_mysql_loc_conf_t, password),
		NULL },

	{	ngx_string("mysql_database"),
		NGX_HTTP_LOC_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(ngx_http_mysql_loc_conf_t, database),
		NULL },

	/* Queries support shell-style substitutions 
	   In the following example $id variable is substituted:

	   SELECT user, count FROM users WHERE id=$id
	 */

	{	ngx_string("mysql_query"),
		NGX_HTTP_LOC_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
		ngx_http_mysql_query,
		NGX_HTTP_LOC_CONF_OFFSET,
		0,
		NULL },

	ngx_null_command
};

/* Module context */
static ngx_http_module_t ngx_http_mysql_module_ctx = {

	NULL,                               /* preconfiguration */
	NULL,                               /* postconfiguration */
	NULL,                               /* create main configuration */
	NULL,                               /* init main configuration */
	NULL,                               /* create server configuration */
	NULL,                               /* merge server configuration */
	ngx_http_mysql_create_loc_conf,     /* create location configuration */
	ngx_http_mysql_merge_loc_conf       /* merge location configuration */
};

/* Module */
ngx_module_t ngx_http_mysql_module = {

	NGX_MODULE_V1,
	&ngx_http_mysql_module_ctx,         /* module context */
	ngx_http_mysql_commands,            /* module directives */
	NGX_HTTP_MODULE,                    /* module type */
	NULL,                               /* init master */
	NULL,                               /* init module */
	NULL,                               /* init process */
	NULL,                               /* init thread */
	NULL,                               /* exit thread */
	NULL,                               /* exit process */
	NULL,                               /* exit master */
	NGX_MODULE_V1_PADDING
};

#define NGXCSTR(s) ((s).data ? strndupa((char*)(s).data, (s).len) : NULL)

ngx_int_t ngx_http_mysql_handler(ngx_http_request_t *r) {

	ngx_http_mysql_loc_conf_t *mslcf;
	ngx_str_t query;
	MYSQL mysql, *sock;
	MYSQL_RES *res;
	MYSQL_ROW row;
	char *value;
	size_t len;
	int n, num_fields;
	ngx_chain_t *out, *node, *prev;
	uint64_t auto_id;

	mslcf = ngx_http_get_module_loc_conf(r, ngx_http_mysql_module);

	if (ngx_http_script_run(r, &query, mslcf->query_lengths->elts, 0,
				mslcf->query_values->elts) == NULL)
	{
		return NGX_ERROR;
	}

	/* TODO: we need connection pool here */
	
	mysql_init(&mysql);

	if (!(sock = mysql_real_connect(&mysql,
				NGXCSTR(mslcf->host), 
				NGXCSTR(mslcf->user), 
				NGXCSTR(mslcf->password),
				NGXCSTR(mslcf->database), 
				mslcf->port == NGX_CONF_UNSET ? 0 : mslcf->port, 
				NULL, 0))) 
	{

		ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, 
				"Couldn't connect to MySQL engine: %s",
				mysql_error(&mysql));
		
		return NGX_ERROR;
	}

	mysql.reconnect = 1;

	if (mysql_query(sock, NGXCSTR(query))) {

		mysql_close(sock);
		
		ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, 
				"MySQL query failed (%s)",
				mysql_error(sock));

		return NGX_ERROR;
	}

	out = NULL;

	if (!mysql_field_count(sock)) {

		/* no data returned 
		 check if there's auto-id pending */

		auto_id = mysql_insert_id(sock);

		if (auto_id) {

			ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, 
					"MySQL auto-inserted id=%uL", auto_id);

			out = (ngx_chain_t*)ngx_palloc(r->pool, sizeof(ngx_chain_t));
			out->next = NULL;

			out->buf = ngx_create_temp_buf(r->pool, 32);
			out->buf->last = ngx_slprintf(out->buf->pos, out->buf->end, "%uL\n", auto_id);

		}

	} else {

		if (!(res = mysql_store_result(sock))) {

			/* no result
			   return auto insert id for the case of insert */

			mysql_close(sock);

			ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, 
					"Couldn't get MySQL result from %s",
					mysql_error(sock));

			return NGX_ERROR;
		}

		num_fields = mysql_num_fields(res);

		prev = NULL;

		while((row = mysql_fetch_row(res))) {

			for(n = 0; n < num_fields; ++n) {

				value = row[n];

				ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, 
					"MySQL value returned '%s'", value);

				len = value ? strlen(value) : 0;

				node = (ngx_chain_t*)ngx_palloc(r->pool, sizeof(ngx_chain_t));
				node->next = NULL;
				node->buf = ngx_create_temp_buf(r->pool, len + 1);

				if (value)
					memcpy(node->buf->pos, value, len);

				node->buf->pos[len] = '\n';
				node->buf->last += ++len;

				if (prev)
					prev->next = node;

				else
					out = node;

				prev = node;
			}
		}
	}

	mysql_free_result(res);
	mysql_close(sock);

	if (out == NULL) {

		/* no result */

		out = (ngx_chain_t*)ngx_palloc(r->pool, sizeof(ngx_chain_t));
		out->next = NULL;
		out->buf = ngx_create_temp_buf(r->pool, 2);
		*out->buf->last++ = '\n';

	}

	ngx_http_send_header(r);

	out->buf->last_buf = 1;
	ngx_http_output_filter(r, out);

	return NGX_OK;
}

static void* ngx_http_mysql_create_loc_conf(ngx_conf_t *cf)
{
	ngx_http_mysql_loc_conf_t *conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_mysql_loc_conf_t));
	
	conf->port = NGX_CONF_UNSET;

	return conf;
}

static char* ngx_http_mysql_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
	ngx_http_mysql_loc_conf_t *prev = parent;
	ngx_http_mysql_loc_conf_t *conf = child;

	ngx_conf_merge_str_value(conf->host,
			prev->host, NULL);

	ngx_conf_merge_value(conf->port,
			prev->port, NGX_CONF_UNSET);
	
	ngx_conf_merge_str_value(conf->user,
			prev->user, NULL);

	ngx_conf_merge_str_value(conf->password,
			prev->password, NULL);

	ngx_conf_merge_str_value(conf->database,
			prev->database, NULL);

	if (conf->query_lengths == NULL)
		conf->query_lengths = prev->query_lengths;

	if (conf->query_values == NULL)
		conf->query_values = prev->query_values;

	return NGX_CONF_OK;
}

static char * ngx_http_mysql_query(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	ngx_http_mtask_loc_conf_t *mlcf = conf;
	ngx_http_mysql_loc_conf_t *mslcf = conf;
	ngx_str_t *query;
	ngx_uint_t n;
	ngx_http_script_compile_t sc;
	ngx_str_t *value;

	mlcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_mtask_module);
	mlcf->handler = &ngx_http_mysql_handler;

	mslcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_mysql_module);

	value = cf->args->elts;
	query = &value[1];

	n = ngx_http_script_variables_count(query);

	ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

	sc.cf = cf;
	sc.source = query;
	sc.lengths = &mslcf->query_lengths;
	sc.values = &mslcf->query_values;
	sc.variables = n;
	sc.complete_lengths = 1;
	sc.complete_values = 1;

	if (ngx_http_script_compile(&sc) != NGX_OK)
		return NGX_CONF_ERROR;

	return NGX_CONF_OK;
}

