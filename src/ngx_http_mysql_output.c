/*************************************************************************
 *   > File Name: ngx_http_mysql_output.h
 *     > Author: DenoFiend
 *       > Mail: denofiend@gmail.com
 *         > Created Time: 2012年12月26日 星期三 16时53分13秒
 *          ************************************************************************/

#ifndef DDEBUG
#define DDEBUG 0
#endif

#include "ngx_http_mysql_ddebug.h"
#include "ngx_http_mysql_module.h"
#include "ngx_http_mysql_output.h"


ngx_int_t ngx_mysql_output_value(ngx_http_request_t *r, MYSQL_RES *res){
    ngx_mysql_ctx_t        *msqlctx;
    ngx_http_core_loc_conf_t  *clcf;
    ngx_chain_t               *cl;
    ngx_buf_t                 *b;
    size_t                     size;

    dd("entering");

    msqlctx = ngx_http_get_module_ctx(r, ngx_mysql_module);

    if ((msqlctx->var_rows != 1) || (msqlctx->var_cols != 1)) {
        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "mysql: \"mysql_output value\" received %d value(s)"
                      " instead of expected single value in location \"%V\"",
                      msqlctx->var_rows * msqlctx->var_cols, &clcf->name);

        dd("returning NGX_DONE, status NGX_HTTP_INTERNAL_SERVER_ERROR");
        msqlctx->status = NGX_HTTP_INTERNAL_SERVER_ERROR;
        return NGX_DONE;
    }

    if (PQgetisnull(res, 0, 0)) {
        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "mysql: \"mysql_output value\" received NULL value"
                      " in location \"%V\"", &clcf->name);

        dd("returning NGX_DONE, status NGX_HTTP_INTERNAL_SERVER_ERROR");
        msqlctx->status = NGX_HTTP_INTERNAL_SERVER_ERROR;
        return NGX_DONE;
    }

    size = PQgetlength(res, 0, 0);
    if (size == 0) {
        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "mysql: \"mysql_output value\" received empty value"
                      " in location \"%V\"", &clcf->name);

        dd("returning NGX_DONE, status NGX_HTTP_INTERNAL_SERVER_ERROR");
        msqlctx->status = NGX_HTTP_INTERNAL_SERVER_ERROR;
        return NGX_DONE;
    }

    b = ngx_create_temp_buf(r->pool, size);
    if (b == NULL) {
        dd("returning NGX_ERROR");
        return NGX_ERROR;
    }

    cl = ngx_alloc_chain_link(r->pool);
    if (cl == NULL) {
        dd("returning NGX_ERROR");
        return NGX_ERROR;
    }

    cl->buf = b;
    b->memory = 1;
    b->tag = r->upstream->output.tag;

    b->last = ngx_copy(b->last, PQgetvalue(res, 0, 0), size);

    if (b->last != b->end) {
        dd("returning NGX_ERROR");
        return NGX_ERROR;
    }

    cl->next = NULL;

    /* set output response */
    msqlctx->response = cl;

    dd("returning NGX_DONE");
    return NGX_DONE;
}

ngx_int_t
ngx_mysql_output_text(ngx_http_request_t *r, MYSQL_RES *res)
{
    ngx_mysql_ctx_t        *msqlctx;
    ngx_chain_t               *cl;
    ngx_buf_t                 *b;
    size_t                     size;
    ngx_int_t                  col_count, row_count, col, row;

    dd("entering");

    msqlctx = ngx_http_get_module_ctx(r, ngx_mysql_module);

    col_count = msqlctx->var_cols;
    row_count = msqlctx->var_rows;

    /* pre-calculate total length up-front for single buffer allocation */
    size = 0;

    for (row = 0; row < row_count; row++) {
        for (col = 0; col < col_count; col++) {
            if (PQgetisnull(res, row, col)) {
                size += sizeof("(null)") - 1;
            } else {
                size += PQgetlength(res, row, col);  /* field string data */
            }
        }
    }

    size += row_count * col_count - 1;               /* delimiters */

    if ((row_count == 0) || (size == 0)) {
        dd("returning NGX_DONE (empty result)");
        return NGX_DONE;
    }

    b = ngx_create_temp_buf(r->pool, size);
    if (b == NULL) {
        dd("returning NGX_ERROR");
        return NGX_ERROR;
    }

    cl = ngx_alloc_chain_link(r->pool);
    if (cl == NULL) {
        dd("returning NGX_ERROR");
        return NGX_ERROR;
    }

    cl->buf = b;
    b->memory = 1;
    b->tag = r->upstream->output.tag;

    /* fill data */
    for (row = 0; row < row_count; row++) {
        for (col = 0; col < col_count; col++) {
            if (PQgetisnull(res, row, col)) {
                b->last = ngx_copy(b->last, "(null)", sizeof("(null)") - 1);
            } else {
                size = PQgetlength(res, row, col);
                if (size) {
                    b->last = ngx_copy(b->last, PQgetvalue(res, row, col),
                                       size);
                }
            }

            if ((row != row_count - 1) || (col != col_count - 1)) {
                b->last = ngx_copy(b->last, "\n", 1);
            }
        }
    }

    if (b->last != b->end) {
        dd("returning NGX_ERROR");
        return NGX_ERROR;
    }

    cl->next = NULL;

    /* set output response */
    msqlctx->response = cl;

    dd("returning NGX_DONE");
    return NGX_DONE;
}

ngx_int_t ngx_mysql_output_rds(ngx_http_request_t *r, MYSQL_RES *res)
{
    ngx_mysql_ctx_t  *msqlctx;
    ngx_chain_t         *first, *last;
    ngx_int_t            col_count, row_count, aff_count, row, errcode, insert_id;
	ngx_str_t errstr;
	MYSQL_FIELD *fields;
	MYSQL_ROW val;
	ngx_uint_t *lengths;

    dd("entering");

    msqlctx = ngx_http_get_module_ctx(r, ngx_http_mysql_module);

    col_count = msqlctx->var_cols;
    row_count = msqlctx->var_rows;
    aff_count = (msqlctx->var_affected == NGX_ERROR) ? 0 : msqlctx->var_affected;
	errcode = msqlctx->errcode;
	insert_id = msqlctx->insert_id;
	errstr = msqlctx->errstr;
	fields = mysql_fetch_fields(res);


    /* render header */
    first = last = ngx_mysql_render_rds_header(r, r->pool, col_count, aff_count, NGXCSTR(errstr), errcode, insert_id);

    if (last == NULL) {
        dd("returning NGX_ERROR");
        return NGX_ERROR;
    }

    if (!errno) {
        goto done;
    }

    /* render columns */
    last->next = ngx_mysql_render_rds_columns(r, r->pool, fields, col_count);
    if (last->next == NULL) {
        dd("returning NGX_ERROR");
        return NGX_ERROR;
    }
    last = last->next;

    /* render rows */
    for (row = 0; row < row_count; row++) {
		val = mysql_fetch_row(res);
		if (NULL == val)
		{
			dd("fetch_row error");
			return NGX_ERROR;
		}	
		lengths = mysql_fetch_lengths(res);

        last->next = ngx_mysql_render_rds_row(r, r->pool, val, lengths, col_count,
                                                 row, (row == row_count - 1));
        if (last->next == NULL) {
            dd("returning NGX_ERROR");
            return NGX_ERROR;
        }
        last = last->next;
    }

    /* render row terminator (for empty result-set only) */
    if (row == 0) {
        last->next = ngx_mysql_render_rds_row_terminator(r, r->pool);
        if (last->next == NULL) {
            dd("returning NGX_ERROR");
            return NGX_ERROR;
        }
        last = last->next;
    }

done:
    last->next = NULL;

    /* set output response */
    msqlctx->response = first;

    dd("returning NGX_DONE");
    return NGX_DONE;
}

ngx_chain_t * ngx_mysql_render_rds_header(ngx_http_request_t *r, ngx_pool_t *pool,
		 ngx_uint_t col_count, ngx_uint_t aff_count, char* errstr, ngx_uint_t errcode, ngx_uint_t insert_id)
{
    ngx_chain_t  *cl;
    ngx_buf_t    *b;
    size_t        size;
    size_t        errstr_len;

    dd("entering");

    errstr_len = ngx_strlen(errstr);

    size = sizeof(uint8_t)        /* endian type */
         + sizeof(uint32_t)       /* format version */
         + sizeof(uint8_t)        /* result type */
         + sizeof(uint16_t)       /* standard error code */
         + sizeof(uint16_t)       /* driver-specific error code */
         + sizeof(uint16_t)       /* driver-specific error string length */
         + (uint16_t) errstr_len  /* driver-specific error string data */
         + sizeof(uint64_t)       /* rows affected */
         + sizeof(uint64_t)       /* insert id */
         + sizeof(uint16_t)       /* column count */
         ;

    b = ngx_create_temp_buf(pool, size);
    if (b == NULL) {
        dd("returning NULL");
        return NULL;
    }

    cl = ngx_alloc_chain_link(pool);
    if (cl == NULL) {
        dd("returning NULL");
        return NULL;
    }

    cl->buf = b;
    b->memory = 1;
    b->tag = r->upstream->output.tag;

    /* fill data */
#if NGX_HAVE_LITTLE_ENDIAN
    *b->last++ = 0;
#else
    *b->last++ = 1;
#endif

    *(uint32_t *) b->last = (uint32_t) resty_dbd_stream_version;
    b->last += sizeof(uint32_t);

    *b->last++ = 0;

    *(uint16_t *) b->last = (uint16_t) 0;
    b->last += sizeof(uint16_t);

    *(uint16_t *) b->last = (uint16_t) errcode;
    b->last += sizeof(uint16_t);

    *(uint16_t *) b->last = (uint16_t) errstr_len;
    b->last += sizeof(uint16_t);

    if (errstr_len) {
        b->last = ngx_copy(b->last, (u_char *) errstr, errstr_len);
    }

    *(uint64_t *) b->last = (uint64_t) aff_count;
    b->last += sizeof(uint64_t);

    *(uint64_t *) b->last = (uint64_t) insert_id;
    b->last += sizeof(uint64_t);

    *(uint16_t *) b->last = (uint16_t) col_count;
    b->last += sizeof(uint16_t);

    if (b->last != b->end) {
        dd("returning NULL");
        return NULL;
    }

    dd("returning");
    return cl;
}

ngx_chain_t * ngx_mysql_render_rds_columns(ngx_http_request_t *r, ngx_pool_t *pool,
    MYSQL_FIELD *fields, ngx_int_t col_count)
{
    ngx_chain_t  *cl;
    ngx_buf_t    *b;
    size_t        size;
    ngx_int_t     col;
    enum_field_types           col_type;
    char         *col_name;
    size_t        col_name_len;

    dd("entering");

    /* pre-calculate total length up-front for single buffer allocation */
    size = col_count
         * (sizeof(uint16_t)    /* standard column type */
            + sizeof(uint16_t)  /* driver-specific column type */
            + sizeof(uint16_t)  /* column name string length */
           );

    for (col = 0; col < col_count; col++) {
        size += ngx_strlen(fields[col].name);  /* column name string data */
    }

    b = ngx_create_temp_buf(pool, size);
    if (b == NULL) {
        dd("returning NULL");
        return NULL;
    }

    cl = ngx_alloc_chain_link(pool);
    if (cl == NULL) {
        dd("returning NULL");
        return NULL;
    }

    cl->buf = b;
    b->memory = 1;
    b->tag = r->upstream->output.tag;

    /* fill data */
    for (col = 0; col < col_count; col++) {
        col_type = fields[col].type;
        col_name = fields[col].name;
        col_name_len = (uint16_t) ngx_strlen(col_name);

        *(uint16_t *) b->last = (uint16_t) ngx_mysql_rds_col_type(col_type);
        b->last += sizeof(uint16_t);

        *(uint16_t *) b->last = col_type;
        b->last += sizeof(uint16_t);

        *(uint16_t *) b->last = col_name_len;
        b->last += sizeof(uint16_t);

        b->last = ngx_copy(b->last, col_name, col_name_len);
    }

    if (b->last != b->end) {
        dd("returning NULL");
        return NULL;
    }

    dd("returning");
    return cl;
}

ngx_chain_t *
ngx_mysql_render_rds_row(ngx_http_request_t *r, ngx_pool_t *pool,
    MYSQL_ROW val, ngx_uint_t *lengths, ngx_int_t col_count, ngx_int_t row, ngx_int_t last_row)
{
    ngx_chain_t  *cl;
    ngx_buf_t    *b;
    size_t        size;
    ngx_int_t     col;

    dd("entering, row:%d", (int) row);

    /* pre-calculate total length up-front for single buffer allocation */
    size = sizeof(uint8_t)                 /* row number */
         + (col_count * sizeof(uint32_t))  /* field string length */
         ;

    if (last_row) {
        size += sizeof(uint8_t);
    }

    for (col = 0; col < col_count; col++) {
        size += lengths[col]  /* field string data */
    }

    b = ngx_create_temp_buf(pool, size);
    if (b == NULL) {
        dd("returning NULL");
        return NULL;
    }

    cl = ngx_alloc_chain_link(pool);
    if (cl == NULL) {
        dd("returning NULL");
        return NULL;
    }

    cl->buf = b;
    b->memory = 1;
    b->tag = r->upstream->output.tag;

    /* fill data */
    *b->last++ = (uint8_t) 1; /* valid row */

    for (col = 0; col < col_count; col++) {
        if (val[col] == NULL) {
            *(uint32_t *) b->last = (uint32_t) -1;
             b->last += sizeof(uint32_t);
        } else {
            size = lengths[col];
            *(uint32_t *) b->last = (uint32_t) size;
            b->last += sizeof(uint32_t);

            if (size) {
                b->last = ngx_copy(b->last, val[col], size);
            }
        }
    }

    if (last_row) {
        *b->last++ = (uint8_t) 0; /* row terminator */
    }

    if (b->last != b->end) {
        dd("returning NULL");
        return NULL;
    }

    dd("returning");
    return cl;
}

ngx_chain_t *
ngx_mysql_render_rds_row_terminator(ngx_http_request_t *r, ngx_pool_t *pool)
{
    ngx_chain_t  *cl;
    ngx_buf_t    *b;

    dd("entering");

    b = ngx_create_temp_buf(pool, sizeof(uint8_t));
    if (b == NULL) {
        dd("returning NULL");
        return NULL;
    }

    cl = ngx_alloc_chain_link(pool);
    if (cl == NULL) {
        dd("returning NULL");
        return NULL;
    }

    cl->buf = b;
    b->memory = 1;
    b->tag = r->upstream->output.tag;

    /* fill data */
    *b->last++ = (uint8_t) 0; /* row terminator */

    if (b->last != b->end) {
        dd("returning NULL");
        return NULL;
    }

    dd("returning");
    return cl;
}

ngx_int_t
ngx_mysql_output_chain(ngx_http_request_t *r, ngx_chain_t *cl)
{
    ngx_http_upstream_t       *u = r->upstream;
    ngx_http_core_loc_conf_t  *clcf;
    ngx_mysql_loc_conf_t   *pglcf;
    ngx_mysql_ctx_t        *msqlctx;
    ngx_int_t                  rc;

    dd("entering");

    if (!r->header_sent) {
        ngx_http_clear_content_length(r);

        pglcf = ngx_http_get_module_loc_conf(r, ngx_mysql_module);
        msqlctx = ngx_http_get_module_ctx(r, ngx_mysql_module);

        r->headers_out.status = msqlctx->status ? abs(msqlctx->status)
                                              : NGX_HTTP_OK;

        if (pglcf->output_handler == &ngx_mysql_output_rds) {
            /* RDS for output rds */
            r->headers_out.content_type.data = (u_char *) rds_content_type;
            r->headers_out.content_type.len = rds_content_type_len;
            r->headers_out.content_type_len = rds_content_type_len;
        } else if (pglcf->output_handler != NULL) {
            /* default type for output value|row */
            clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

            r->headers_out.content_type = clcf->default_type;
            r->headers_out.content_type_len = clcf->default_type.len;
        }

        r->headers_out.content_type_lowcase = NULL;

        rc = ngx_http_send_header(r);
        if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
            dd("returning rc:%d", (int) rc);
            return rc;
        }
    }

    if (cl == NULL) {
        dd("returning NGX_DONE");
        return NGX_DONE;
    }

    rc = ngx_http_output_filter(r, cl);
    if (rc == NGX_ERROR || rc > NGX_OK) {
        dd("returning rc:%d", (int) rc);
        return rc;
    }

#if defined(nginx_version) && (nginx_version >= 1001004)
    ngx_chain_update_chains(r->pool, &u->free_bufs, &u->busy_bufs, &cl,
                            u->output.tag);
#else
    ngx_chain_update_chains(&u->free_bufs, &u->busy_bufs, &cl, u->output.tag);
#endif

    dd("returning rc:%d", (int) rc);
    return rc;
}

rds_col_type_t ngx_mysql_rds_col_type(enum_field_types col_type)
{
    switch (col_type) {
    case MYSQL_TYPE_LONGLONG: /* int8 */
        return rds_col_type_bigint;
    case MYSQL_TYPE_BIT: /* bit */
        return rds_col_type_bit;
    case MYSQL_TYPE_TINY: /* bool */
        return rds_col_type_bool;
    case MYSQL_TYPE_STRING: /* char */
        return rds_col_type_char;
    case MYSQL_TYPE_VAR_STRING: /* varchar */
        return rds_col_type_varchar;
    case MYSQL_TYPE_DATE: /* date */
        return rds_col_type_date;
    case MYSQL_TYPE_DOUBLE: /* float8 */
        return rds_col_type_double;
    case MYSQL_TYPE_LONG: /* int4 */
        return rds_col_type_integer;
    case MYSQL_TYPE_DECIMAL: /* numeric */
    case MYSQL_TYPE_NEWDECIMALL: /* new numeric */
        return rds_col_type_decimal;
    case MYSQL_TYPE_FLOAT: /* float4 */
        return rds_col_type_real;
    case MYSQL_TYPE_SHORT: /* int2 */
        return rds_col_type_smallint;
    case MYSQL_TYPE_TIME: /* time */
        return rds_col_type_time;
    case MYSQL_TYPE_TIMESTAMP: /* timestamp */
        return rds_col_type_timestamp;
    case MYSQL_TYPE_BLOB: /* bytea */
        return rds_col_type_blob;
    default:
        return rds_col_type_unknown;
    }
}

