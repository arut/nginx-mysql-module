/*************************************************************************
  > File Name: ngx_http_mysql_handler.c
  > Author: DenoFiend
  > Mail: denofiend@gmail.com
  > Created Time: 2012年12月27日 星期四 10时55分06秒
 ************************************************************************/

#include "ngx_http_mysql_module.h"
#include "ngx_http_mysql_handler.h"
#include "ngx_http_mysql_output.h"
#include "ngx_http_mysql_ddebug.h"
#include <mysql.h>


ngx_str_t* ngx_strdup(ngx_pool_t *pool, ngx_str_t* dst, const char* src, size_t len)
{
	dst->data = ngx_pnalloc(pool, len);
	dst->len = len;

	if (dst->data == NULL) {
		return NULL;
	}

	ngx_memcpy(dst->data, src, len);

	return dst;
}

ngx_int_t ngx_http_mysql_process_response(ngx_http_request_t *r){

        ngx_http_mysql_loc_conf_t      *mysqlcf;
        ngx_http_mysql_ctx_t           *mysqlctx;
        MYSQL_RES *result;

        dd("entering");

        mysqlcf = ngx_http_get_module_loc_conf(r, ngx_http_mysql_module);
        mysqlctx = ngx_http_get_module_ctx(r, ngx_http_mysql_module);

        result = mysql_store_result(mysqlctx->current);

        if (result)
        {
                /* set $http_mysql_columns */
                mysqlctx->var_cols = mysql_num_fields(result);

                /* set $http_mysql_rows */
                mysqlctx->var_rows = mysql_num_rows(result);
        }
        else
        {
                mysqlctx->var_cols = mysql_field_count(mysqlctx->current);
        }

        /* set $http_mysql_affected */
        mysqlctx->var_affected = mysql_affected_rows(mysqlctx->current);


        if (mysqlcf->output_handler) {
                /* generate output */
                dd("returning");
                return mysqlcf->output_handler(r, result);
        }

        dd("returning NGX_DONE");
        return NGX_DONE;
}


ngx_int_t ngx_http_mysql_transaction_handler(ngx_http_request_t *r){

	ngx_http_mysql_loc_conf_t *mslcf;
	ngx_http_mysql_srv_conf_t *msscf;
	ngx_http_mysql_trans_loc_conf_t * transaction_sqls;
	ngx_str_t trans_lev = ngx_string("SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED");
	MYSQL mysql, *sock;
	ngx_http_mysql_node_t *mnode;
	ngx_http_mysql_ctx_t *ctx;
	ngx_int_t ret;
	ngx_uint_t i;
	ngx_str_t query;
	const char* errs;

	msscf = ngx_http_get_module_srv_conf(r, ngx_http_mysql_module);

	mslcf = ngx_http_get_module_loc_conf(r, ngx_http_mysql_module);

	ctx = ngx_http_get_module_ctx(r, ngx_http_mysql_module);

	if (ctx == NULL) {

		ctx = ngx_pcalloc(r->connection->pool, sizeof(ngx_http_mysql_ctx_t));

		ngx_http_set_ctx(r, ctx, ngx_http_mysql_module);
	}

	sock = NULL;
	mnode = NULL;
	ret = NGX_ERROR;

	if (msscf->max_conn != NGX_CONF_UNSET 
			&& msscf->max_conn) 
	{
		/* take connection from pool */

		if (msscf->free_node == NULL) {

			ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0, 
					"not enough MySQL connections: %d", msscf->max_conn);

			goto quit;
		}

		mnode = msscf->free_node;

		msscf->free_node = msscf->free_node->next;

		sock = &mnode->mysql;

	} else {

		/* connection-per-request */

		sock = &mysql;
	}

	if (mnode == NULL || !mnode->ready) {

		ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, 
				"connecting to MySQL");

		mysql_init(sock);

		if (!(sock = mysql_real_connect(sock,
						NGXCSTR(msscf->host), 
						NGXCSTR(msscf->user), 
						NGXCSTR(msscf->password),
						NGXCSTR(msscf->database), 
						msscf->port == NGX_CONF_UNSET ? 0 : msscf->port, 
						NULL,
						msscf->multi == 1 ? CLIENT_MULTI_STATEMENTS : 0))) 
		{

			/* for output errs, erro */
			errs = mysql_error(sock);
			ngx_strdup(r->connection->pool, &ctx->errstr, errs, strlen(errs));
			ctx->errcode = mysql_errno(sock);

			ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0, 
					"couldn't connect to MySQL engine: %s", errs);

			goto quit;
		}

		/*set charset*/
		if (msscf->charset.len 
				&& mysql_set_character_set(sock, NGXCSTR(msscf->charset)))
		{
			/* for output errs, erro */
			errs = mysql_error(sock);
			ngx_strdup(r->connection->pool, &ctx->errstr, errs, strlen(errs));
			ctx->errcode = mysql_errno(sock);

			ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0, 
					"error setting MySQL charset: %s", errs);

			goto quit;
		}

		sock->reconnect = 1;

		if (mnode)
			mnode->ready = 1;

		ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "mysql_auto_commit(%d)", msscf->mysql_auto_commit);

		/*set auto_commit*/
		if (0 != mysql_autocommit(sock, msscf->mysql_auto_commit))
		{
			/* for output errs, erro */
			errs = mysql_error(sock);
			ngx_strdup(r->connection->pool, &ctx->errstr, errs, strlen(errs));
			ctx->errcode = mysql_errno(sock);

			ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0, 
					"error setting MySQL auto_commit: %s", errs);

			goto quit;
		}

		/* set TRANSACTION_REPEATABLE_READ to trans level*/
		if (0 != mysql_real_query(sock, NGXCSTR(trans_lev), trans_lev.len))
		{
			/* for output errs, erro */
			errs = mysql_error(sock);
			ngx_strdup(r->connection->pool, &ctx->errstr, errs, strlen(errs));
			ctx->errcode = mysql_errno(sock);

			ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0, 
					"error setting MySQL transaction level: %s", errs);

			goto quit;
		}
	}


	/* set current MySQL connection for escapes */

	ctx->current = sock;


	transaction_sqls = mslcf->transaction_sqls;

	ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "query len(%d)", transaction_sqls->len);

	for (i = 0; i < transaction_sqls->len-1; ++i) 
	{

		//ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "transaction_sql sql[%d]:%V", i, &transaction_sqls[i].sql); 

		/* not found script $arg_id in sql */
		if (NULL == transaction_sqls[i].query_lengths && NULL == transaction_sqls[i].query_values)
		{
			query = transaction_sqls[i].sql;
		}
		else
		{
			if (ngx_http_script_run(r, &query, transaction_sqls[i].query_lengths->elts, 0, transaction_sqls[i].query_values->elts) == NULL)
			{
				ctx->current = NULL;

				goto quit;
			}

		}

		//ctx->current = NULL;

		ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "transaction_sql query[%d]:%V", i, &query); 

		if (mysql_real_query(sock, NGXCSTR(query), query.len)) 
		{

			/* for output errs, erro */
			errs = mysql_error(sock);
			ngx_strdup(r->connection->pool, &(ctx->errstr), errs, strlen(errs));
			ctx->errcode = mysql_errno(sock);

			ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0, 
					"MySQL read_query failed (%V)", &(ctx->errstr));
			
			ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "transaction_sql query[%d]:%s", i+1, "ROLL_BACK"); 

			
			if (mysql_rollback(sock))
			{
				/* for output errs, erro */
				errs = mysql_error(sock);
				ngx_strdup(r->connection->pool, &ctx->errstr, errs, strlen(errs));
				ctx->errcode = mysql_errno(sock);

				ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0, 
						"MySQL roll_back failed (%s)", errs);
			}

			goto quit;
		}
	}

	ret = ngx_http_mysql_process_response(r);

	if (ret != NGX_DONE)
	{
		goto quit;
	}

	ctx->current = NULL;

	return ngx_mysql_output_chain(r, ctx->response);

quit:

	if (mnode != NULL) {

		mnode->next = msscf->free_node;
		msscf->free_node = mnode;
	}
	else if (sock != NULL) {

		mysql_close(sock);
	}
	if (r->subrequest_in_memory) {

		ctx->subreq_out = ctx->response;
	}

	ngx_http_mysql_process_response(r);

	return ngx_mysql_output_chain(r, ctx->response);
}

ngx_int_t ngx_http_mysql_handler(ngx_http_request_t *r) {

	/* NB:
	   Remember this handler is completely ASYNCHRONOUS 
	   when used with nginx-mtask-module;
	   Blocking calls are intercepted to switch user-space
	   context to NGINX event cycle.
	 */

	ngx_http_mysql_loc_conf_t *mslcf;
	ngx_http_mysql_srv_conf_t *msscf;
	ngx_str_t query;
	MYSQL mysql, *sock;
	ngx_http_mysql_node_t *mnode;
	ngx_int_t ret;
	ngx_http_mysql_ctx_t *ctx;
	const char* errs;

	msscf = ngx_http_get_module_srv_conf(r, ngx_http_mysql_module);
	
	mslcf = ngx_http_get_module_loc_conf(r, ngx_http_mysql_module);

	ctx = ngx_http_get_module_ctx(r, ngx_http_mysql_module);

	if (ctx == NULL) {
	
		ctx = ngx_pcalloc(r->connection->pool, sizeof(ngx_http_mysql_ctx_t));

		ngx_http_set_ctx(r, ctx, ngx_http_mysql_module);
	}

	sock = NULL;
	mnode = NULL;
	ret = NGX_ERROR;

	if (msscf->max_conn != NGX_CONF_UNSET 
			&& msscf->max_conn) 
	{
		/* take connection from pool */

		if (msscf->free_node == NULL) {

			ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0, 
					"not enough MySQL connections: %d", msscf->max_conn);

			goto quit;
		}

		mnode = msscf->free_node;

		msscf->free_node = msscf->free_node->next;

		sock = &mnode->mysql;

	} else {

		/* connection-per-request */

		sock = &mysql;
	}

	if (mnode == NULL || !mnode->ready) {

		ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, 
				"connecting to MySQL");

		mysql_init(sock);

		if (!(sock = mysql_real_connect(sock,
						NGXCSTR(msscf->host), 
						NGXCSTR(msscf->user), 
						NGXCSTR(msscf->password),
						NGXCSTR(msscf->database), 
						msscf->port == NGX_CONF_UNSET ? 0 : msscf->port, 
						NULL,
						msscf->multi == 1 ? CLIENT_MULTI_STATEMENTS : 0))) 
		{

			/* for output errs, erro */
			errs = mysql_error(sock);
			ngx_strdup(r->connection->pool, &ctx->errstr, errs, strlen(errs));
			ctx->errcode = mysql_errno(sock);

			ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0, 
					"couldn't connect to MySQL engine: %s", errs);

			goto quit;
		}

		if (msscf->charset.len 
				&& mysql_set_character_set(sock, NGXCSTR(msscf->charset)))
		{
			/* for output errs, erro */
			errs = mysql_error(sock);
			ngx_strdup(r->connection->pool, &ctx->errstr, errs, strlen(errs));
			ctx->errcode = mysql_errno(sock);

			ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0, 
					"error setting MySQL charset: %s", errs);

			goto quit;
		}

		sock->reconnect = 1;

		if (mnode)
			mnode->ready = 1;
	}

	/* set current MySQL connection for escapes */

	ctx->current = sock;

	if (ngx_http_script_run(r, &query, mslcf->query_lengths->elts, 0,
				mslcf->query_values->elts) == NULL)
	{
		ctx->current = NULL;

		goto quit;
	}

	//ctx->current = NULL;

	if (mysql_query(sock, NGXCSTR(query))) {

		/* for output errs, erro */
		errs = mysql_error(sock);
		ngx_strdup(r->connection->pool, &ctx->errstr, errs, strlen(errs));
		ctx->errcode = mysql_errno(sock);

		ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0, 
				"MySQL query failed (%s)", errs);

		goto quit;
	}

	ret = ngx_http_mysql_process_response(r);

	if (ret != NGX_DONE)
	{
		goto quit;
	}

	return ngx_mysql_output_chain(r, ctx->response);

quit:

	if (mnode != NULL) {

		mnode->next = msscf->free_node;
		msscf->free_node = mnode;

	} else if (sock != NULL)
		mysql_close(sock);

	if (r->subrequest_in_memory) {

		ctx->subreq_out = ctx->response;
	}


	ngx_http_mysql_process_response(r);
	return ngx_mysql_output_chain(r, ctx->response);
}

ngx_int_t ngx_http_mysql_subrequest_handler(ngx_http_request_t *r)
{
	ngx_http_mysql_loc_conf_t *mslcf;
	ngx_http_request_t *sr;
	ngx_http_mysql_ctx_t *ctx;
	u_char *q;
	ngx_str_t uri;
	ngx_str_t args = ngx_null_string;
	
	mslcf = ngx_http_get_module_loc_conf(r, ngx_http_mysql_module);

	ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, 
			"mysql subrequest handler");

	if (mslcf->subreq_values == NULL
			|| mslcf->subreq_lengths == NULL)
	{
		return NGX_DECLINED;
	}

	ctx = ngx_http_get_module_ctx(r, ngx_http_mysql_module);

	if (ctx != NULL)
		return NGX_DECLINED;

	if (ngx_http_script_run(r, &uri, mslcf->subreq_lengths->elts, 0,
				mslcf->subreq_values->elts) == NULL)
	{
		return NGX_ERROR;
	}

	ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, 
			"mysql subrequest handler '%V'", &uri);

	q = ngx_strlchr(uri.data, uri.data + uri.len, '?');

	if (q != NULL) {
		args.data = q + 1;
		args.len = uri.data + uri.len - q - 1;
		uri.len = q - uri.data;
	}

	if (ngx_http_subrequest(r, 
			&uri,
			&args,
			&sr,
			NULL,
			NGX_HTTP_SUBREQUEST_WAITED|NGX_HTTP_SUBREQUEST_IN_MEMORY) != NGX_OK)
	{
		return NGX_ERROR;
	}

	if (sr == NULL)
		return NGX_ERROR;

	/* set the same context to parent & subrequest;
	   result will go there */

	ctx = ngx_pcalloc(r->connection->pool, sizeof(ngx_http_mysql_ctx_t));

	ngx_http_set_ctx(r, ctx, ngx_http_mysql_module);
	ngx_http_set_ctx(sr, ctx, ngx_http_mysql_module);

	return NGX_DONE;
}


