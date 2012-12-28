/*************************************************************************
  > File Name: ngx_http_mysql_module.c
  > Author: DenoFiend
  > Mail: denofiend@gmail.com
  > Created Time: 2012年12月21日 星期五 15时46分57秒
 ************************************************************************/

#include "ngx_http_mysql_module.h"
#include "ngx_http_mysql_output.h"
#include "ngx_http_mysql_ddebug.h"
#include "ngx_http_mysql_handler.h"

#define NGX_CONF_TAKE34567 (NGX_CONF_TAKE3|NGX_CONF_TAKE4|NGX_CONF_TAKE5|NGX_CONF_TAKE6|NGX_CONF_TAKE7)  

static ngx_command_t ngx_http_mysql_commands[] = {

	{	ngx_string("mysql_host"),
		NGX_HTTP_SRV_CONF|NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_SRV_CONF_OFFSET,
		offsetof(ngx_http_mysql_srv_conf_t, host),
		NULL },

	{	ngx_string("mysql_port"),
		NGX_HTTP_SRV_CONF|NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_num_slot,
		NGX_HTTP_SRV_CONF_OFFSET,
		offsetof(ngx_http_mysql_srv_conf_t, port),
		NULL },

	{	ngx_string("mysql_user"),
		NGX_HTTP_SRV_CONF|NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_SRV_CONF_OFFSET,
		offsetof(ngx_http_mysql_srv_conf_t, user),
		NULL },

	{	ngx_string("mysql_password"),
		NGX_HTTP_SRV_CONF|NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_SRV_CONF_OFFSET,
		offsetof(ngx_http_mysql_srv_conf_t, password),
		NULL },

	{	ngx_string("mysql_database"),
		NGX_HTTP_SRV_CONF|NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_SRV_CONF_OFFSET,
		offsetof(ngx_http_mysql_srv_conf_t, database),
		NULL },

	{	ngx_string("mysql_charset"),
		NGX_HTTP_SRV_CONF|NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_SRV_CONF_OFFSET,
		offsetof(ngx_http_mysql_srv_conf_t, charset),
		NULL },

	{	ngx_string("mysql_connections"),
		NGX_HTTP_SRV_CONF|NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_num_slot,
		NGX_HTTP_SRV_CONF_OFFSET,
		offsetof(ngx_http_mysql_srv_conf_t, max_conn),
		NULL },

	{	ngx_string("mysql_multi"),
		NGX_HTTP_SRV_CONF|NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_flag_slot,
		NGX_HTTP_SRV_CONF_OFFSET,
		offsetof(ngx_http_mysql_srv_conf_t, multi),
		NULL },

	{	ngx_string("mysql_auto_commit"),
		NGX_HTTP_SRV_CONF|NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_flag_slot,
		NGX_HTTP_SRV_CONF_OFFSET,
		offsetof(ngx_http_mysql_srv_conf_t, mysql_auto_commit),
		NULL },

	/* Queries support shell-style substitutions 
	   In the following example $id variable is substituted:

	   SELECT user, count FROM users WHERE id=$id
	 */

	{	ngx_string("mysql_query"),
		NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE1,
		ngx_http_mysql_query,
		NGX_HTTP_LOC_CONF_OFFSET,
		0,
		NULL },

	{	ngx_string("mysql_subrequest"),
		NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_1MORE,
		ngx_http_mysql_subrequest,
		NGX_HTTP_LOC_CONF_OFFSET,
		0,
		NULL },

	{	ngx_string("mysql_escape"),
		NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE1234,
		ngx_http_mysql_escape,
		NGX_HTTP_LOC_CONF_OFFSET,
		0,
		NULL },
	
	{	ngx_string("mysql_transaction"),
		NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE34567,
		ngx_http_mysql_transaction,
		NGX_HTTP_LOC_CONF_OFFSET,
		0,
		NULL },

	ngx_null_command
};

/* Module context */
static ngx_http_module_t ngx_http_mysql_module_ctx = {

	NULL,                               /* preconfiguration */
	ngx_http_mysql_init,                /* postconfiguration */
	NULL,                               /* create main configuration */
	NULL,                               /* init main configuration */
	ngx_http_mysql_create_srv_conf,     /* create server configuration */
	ngx_http_mysql_merge_srv_conf,      /* merge server configuration */
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


ngx_int_t ngx_http_mysql_get_subrequest_variable(ngx_http_request_t *r,
		    ngx_http_variable_value_t *v, uintptr_t data)
{
	ngx_http_mysql_ctx_t *ctx;
	unsigned n = (unsigned)data;
	ngx_chain_t *chain;
	ngx_buf_t *buf;

	ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, 
			"mysql subrequest accessing variable #%d", n);

	ctx = ngx_http_get_module_ctx(r, ngx_http_mysql_module);

	if (ctx != NULL) {

		/* output chain is organized line by line;
		   we need to choose n-th line/buffer from it.
		   remember this method is not supposed to
		   extract tons of fields from request! */

		for(chain = ctx->subreq_out; 
				n && chain != NULL && !chain->buf->last_buf; 
				chain = chain->next, --n);

		if (!n && chain != NULL && chain->buf != NULL) {

			buf = chain->buf;

			v->data = buf->pos;
			v->len = buf->last - buf->pos;

			if (buf->pos != buf->last && buf->last[-1] == '\n')
				--v->len;

			v->valid = 1;

			ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, 
				"mysql subrequest storing variable #%d value: '%*s'", 
				n, v->len, v->data);
		}

	} else
		v->not_found = 1;

	return NGX_OK;
}

ngx_int_t ngx_http_mysql_get_escaped_variable(ngx_http_request_t *r,
		    ngx_http_variable_value_t *v, uintptr_t data)
{
	ngx_http_mysql_ctx_t *ctx;
	ngx_int_t index = (ngx_int_t)data;
	ngx_http_variable_value_t *vv;

	ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, 
			"mysql escaping variable #%d", index);

	ctx = ngx_http_get_module_ctx(r, ngx_http_mysql_module);

	if (ctx != NULL && ctx->current != NULL) {

		vv = ngx_http_get_indexed_variable(r, index);

		/* this size is advised in MySQL docs */
		v->data = ngx_palloc(r->pool, vv->len * 2 + 1);

		v->len = mysql_real_escape_string(ctx->current, (char*)v->data, 
				(const char*)vv->data, vv->len);

		v->valid = 1;

	} else {

		v->not_found = 1;

		ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0, 
					"mysql escaped variable accessed outside of query");
	}

	return NGX_OK;
}

void* ngx_http_mysql_create_srv_conf(ngx_conf_t *cf)
{
	ngx_http_mysql_srv_conf_t *conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_mysql_srv_conf_t));
	
	conf->port = NGX_CONF_UNSET;

	conf->max_conn = NGX_CONF_UNSET;

	conf->multi = NGX_CONF_UNSET;

	conf->mysql_auto_commit = NGX_CONF_UNSET;

	return conf;
}

char* ngx_http_mysql_merge_srv_conf(ngx_conf_t *cf, void *parent, void *child)
{
	ngx_http_mysql_srv_conf_t *prev = parent;
	ngx_http_mysql_srv_conf_t *conf = child;
	ngx_int_t n;

	ngx_log_debug0(NGX_LOG_INFO, cf->log, 0, "mysql merge srv");

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


	if (conf->max_conn != NGX_CONF_UNSET) {

		conf->nodes = ngx_pcalloc(cf->pool, 
				conf->max_conn * sizeof(ngx_http_mysql_node_t));

		conf->free_node = conf->nodes;

		for(n = 0; n < conf->max_conn - 1; ++n)
			conf->nodes[n].next = conf->nodes + n + 1;

	} else if (prev->max_conn != NGX_CONF_UNSET) {

		conf->max_conn = prev->max_conn;

		conf->nodes = prev->nodes;

		conf->free_node = conf->nodes;
	}

	return NGX_CONF_OK;
}

void* ngx_http_mysql_create_loc_conf(ngx_conf_t *cf)
{
	ngx_http_mysql_loc_conf_t *conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_mysql_loc_conf_t));

	conf->output_handler = NGX_CONF_UNSET_PTR;
	
	return conf;
}

char* ngx_http_mysql_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
	ngx_http_mysql_loc_conf_t *prev = parent;
	ngx_http_mysql_loc_conf_t *conf = child;

	ngx_log_debug0(NGX_LOG_INFO, cf->log, 0, "mysql merge loc");



	if (conf->output_handler == NGX_CONF_UNSET_PTR) {
		if (prev->output_handler == NGX_CONF_UNSET_PTR) {
			/* default */
			conf->output_handler = ngx_mysql_output_rds;
		} else {
			/* merge */
			conf->output_handler = prev->output_handler;
		}
	}


	if (conf->query_lengths == NULL)
		conf->query_lengths = prev->query_lengths;

	if (conf->query_values == NULL)
		conf->query_values = prev->query_values;

	if (conf->transaction_sqls == NULL)
		conf->transaction_sqls = prev->transaction_sqls;


	if (conf->subreq_lengths == NULL)
		conf->subreq_lengths = prev->subreq_lengths;

	if (conf->subreq_values == NULL)
		conf->subreq_values = prev->subreq_values;

	if (conf->transaction_sqls == NULL)
		conf->transaction_sqls = prev->transaction_sqls;

	return NGX_CONF_OK;
}

char* ngx_http_mysql_query(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	ngx_http_mysql_loc_conf_t *mslcf = conf;
	ngx_http_mtask_loc_conf_t *mlcf;
	ngx_str_t *query;
	ngx_uint_t n;
	ngx_http_script_compile_t sc;
	ngx_str_t *value;
	mlcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_mtask_module);
	mlcf->handler = &ngx_http_mysql_handler;

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

char* ngx_http_mysql_transaction(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	ngx_http_mysql_loc_conf_t *mslcf = conf;
	ngx_http_mtask_loc_conf_t *mlcf;
	ngx_uint_t n;
	ngx_http_script_compile_t sc;
	ngx_str_t *value;
	ngx_uint_t i;
	ngx_http_mysql_trans_loc_conf_t *mtlcf;


	mlcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_mtask_module);
	mlcf->handler = &ngx_http_mysql_transaction_handler;


	if (mslcf->transaction_sqls) {
		return "is duplicate";
	}

	mtlcf = ngx_pcalloc(cf->pool, cf->args->nelts * sizeof(ngx_http_mysql_trans_loc_conf_t));

	if (mtlcf == NULL) {
		return NGX_CONF_ERROR;
	}

	mslcf->transaction_sqls = mtlcf;
	mtlcf->len = cf->args->nelts;

	value = cf->args->elts;

	for (i = 0; i < cf->args->nelts - 1; i++) {

		mtlcf[i].sql = value[i + 1];

		if (mtlcf[i].sql.data[mtlcf[i].sql.len - 1] == '/') {
			mtlcf[i].sql.len--;
			mtlcf[i].sql.data[mtlcf[i].sql.len] = '\0';
		}

		n = ngx_http_script_variables_count(&mtlcf[i].sql);

		if (n) {
			ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

			sc.cf = cf;
			sc.source = &mtlcf[i].sql;
			sc.lengths = &mtlcf[i].query_lengths;
			sc.values = &mtlcf[i].query_values;
			sc.variables = n;
			sc.complete_lengths = 1;
			sc.complete_values = 1;

			if (ngx_http_script_compile(&sc) != NGX_OK) {
				return NGX_CONF_ERROR;
			}

		} else {
			/* add trailing '\0' to length */
			mtlcf[i].sql.len++;
		}
	}

	return NGX_CONF_OK;
}

char* ngx_http_mysql_subrequest(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	ngx_http_mysql_loc_conf_t *mslcf = conf;
	ngx_str_t *value;
	ngx_uint_t n;
	ngx_http_variable_t *v;
	ngx_http_script_compile_t sc;
	ngx_str_t *uri;

	value = cf->args->elts;

	/* compile script for uri */
	uri = &value[1];

	ngx_log_debug1(NGX_LOG_INFO, cf->log, 0, "mysql subrequest uri: '%V'", uri);

	n = ngx_http_script_variables_count(uri);

	ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

	sc.cf = cf;
	sc.source = uri;
	sc.lengths = &mslcf->subreq_lengths;
	sc.values = &mslcf->subreq_values;
	sc.variables = n;
	sc.complete_lengths = 1;
	sc.complete_values = 1;

	if (ngx_http_script_compile(&sc) != NGX_OK)
		return NGX_CONF_ERROR;

	/* process output variables */
	for(n = 2; n < cf->args->nelts; ++n) {

		if (value[n].len > 0 && value[n].data[0] == '$') {
			++value[n].data;
			--value[n].len;
		}

		v = ngx_http_add_variable(cf, &value[n], NGX_HTTP_VAR_CHANGEABLE);

		v->get_handler = &ngx_http_mysql_get_subrequest_variable;
		v->data = n - 2;
	}

	return NGX_CONF_OK;
}

char* ngx_http_mysql_escape(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	ngx_str_t *value;
	ngx_http_variable_t *v;
	ngx_int_t index;

	value = cf->args->elts;

	if (value[1].data[0] != '$')
		return "needs variable as the first argument";

	value[1].data++;
	value[1].len--;

	if (value[2].data[0] != '$')
		return "needs variable as the second argument";

	value[2].data++;
	value[2].len--;

	index = ngx_http_get_variable_index(cf, &value[2]);

	if (index == NGX_ERROR)
		return "failed to access second variable";

	v = ngx_http_add_variable(cf, &value[1], NGX_HTTP_VAR_CHANGEABLE);

	if (v == NULL)
		return "failed to add variable";

	v->get_handler = ngx_http_mysql_get_escaped_variable;
	v->data = (uintptr_t)index;

	return NGX_CONF_OK;
}

ngx_int_t ngx_http_mysql_init(ngx_conf_t *cf) 
{
	ngx_http_core_main_conf_t *cmcf;
	ngx_http_handler_pt *h;

	cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

	h = ngx_array_push(&cmcf->phases[NGX_HTTP_REWRITE_PHASE].handlers);

	if (h == NULL)
		return NGX_ERROR;

	*h = ngx_http_mysql_subrequest_handler;

	return NGX_OK;
}
