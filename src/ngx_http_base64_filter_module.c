/*
 * Copyright (C) Monkey Zhang (timebug)
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


#define B64_F_LINE_LEN 76

#define B64_NO_FLUSH 0
#define B64_FINISH 1


typedef struct {
    ngx_flag_t enable;
    ssize_t max_length;
} ngx_http_base64_conf_t;


typedef struct {
    ngx_str_t in;
    ngx_str_t out;
} b64_stream;


typedef struct {
    ngx_chain_t *in;
    ngx_chain_t *free;
    ngx_chain_t *busy;
    ngx_chain_t *out;
    ngx_chain_t **last_out;

    ngx_chain_t *copied;
    ngx_chain_t *copy_buf;

    ngx_buf_t *in_buf;
    ngx_buf_t *out_buf;

    unsigned flush:4;
    unsigned done:1;
    unsigned started:1;

    b64_stream b64stream;

    size_t b64cnt;
    u_char b64end[2];
    size_t b64end_n;

} ngx_http_base64_ctx_t;

static ngx_int_t ngx_http_base64_filter_init(ngx_conf_t *cf);
static void *ngx_http_base64_create_conf(ngx_conf_t *cf);
static char *ngx_http_base64_merge_conf(ngx_conf_t *cf, void *parent, void *child);

static ngx_int_t ngx_http_base64_filter_encode_start(ngx_http_request_t *r,
                                                     ngx_http_base64_ctx_t *ctx);
static ngx_int_t ngx_http_base64_filter_add_data(ngx_http_request_t *r,
                                                 ngx_http_base64_ctx_t *ctx);
static ngx_int_t ngx_http_base64_filter_get_buf(ngx_http_request_t *r,
                                                ngx_http_base64_ctx_t *ctx);
static ngx_int_t ngx_http_base64_filter_encode(ngx_http_request_t *r,
                                               ngx_http_base64_ctx_t *ctx);
static void ngx_http_base64_filter_free_copy_buf(ngx_http_request_t *r,
                                                 ngx_http_base64_ctx_t *ctx);


static ngx_command_t ngx_http_base64_filter_commands[] = {

    { ngx_string("base64"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_base64_conf_t, enable),
      NULL },

    { ngx_string("base64_max_length"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_base64_conf_t, max_length),
      NULL },

      ngx_null_command
};


static ngx_http_module_t ngx_http_base64_filter_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_base64_filter_init,           /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_base64_create_conf,           /* create location configuration */
    ngx_http_base64_merge_conf             /* merge location configuration */
};


ngx_module_t ngx_http_base64_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_base64_filter_module_ctx,    /* module context */
    ngx_http_base64_filter_commands,       /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_http_output_header_filter_pt ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt   ngx_http_next_body_filter;

static ngx_int_t
ngx_http_base64_header_filter(ngx_http_request_t *r)
{
    ngx_table_elt_t        *h;
    ngx_http_base64_conf_t *conf;
    ngx_http_base64_ctx_t  *ctx;

    conf = ngx_http_get_module_loc_conf(r, ngx_http_base64_filter_module);

    if (!conf->enable
        || (r->headers_out.status != NGX_HTTP_OK
            && r->headers_out.status != NGX_HTTP_FORBIDDEN
            && r->headers_out.status != NGX_HTTP_NOT_FOUND)
        || (r->headers_out.content_encoding
            && r->headers_out.content_encoding->value.len)
        || (r->headers_out.content_length_n != -1
            && r->headers_out.content_length_n > conf->max_length)
        || r->header_only)
    {
        return ngx_http_next_header_filter(r);
    }

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_base64_ctx_t));
    if (ctx == NULL) {
        return NGX_ERROR;
    }

    ctx->b64end[0] = '\0';
    ctx->b64end[1] = '\0';
    ctx->b64cnt = 0;
    ctx->b64end_n = 0;

    ngx_http_set_ctx(r, ctx, ngx_http_base64_filter_module);

    /* set the 'Content-type' header */
    r->headers_out.content_type_len = sizeof("text/plain") - 1;
    r->headers_out.content_type.len = sizeof("text/plain") - 1;
    r->headers_out.content_type.data = (u_char *) "text/plain";

    size_t size = 0, total = 0, linenums = 0;
    if (r->headers_out.content_length_n != -1) {
        size = r->headers_out.content_length_n;
        total = (( size + 2) / 3) * 4;
        linenums = (total + ( B64_F_LINE_LEN - 1)) / B64_F_LINE_LEN;
        total += 2 * linenums;
        r->headers_out.content_length_n = total;
    }

    h = ngx_list_push(&r->headers_out.headers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    h->hash = 1;
    ngx_str_set(&h->key, "Content-Transfer-Encoding");
    ngx_str_set(&h->value, "base64");

    r->main_filter_need_in_memory = 1;

    return ngx_http_next_header_filter(r);
}


static ngx_int_t
ngx_http_base64_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    int                    rc;
    ngx_http_base64_ctx_t *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_base64_filter_module);

    if (ctx == NULL || ctx->done) {
        return ngx_http_next_body_filter(r, in);
    }

    if (!ctx->started) {
        if (ngx_http_base64_filter_encode_start(r, ctx) != NGX_OK) {
            goto failed;
        }
    }

    if (in) {
        if (ngx_chain_add_copy(r->pool, &ctx->in, in) != NGX_OK) {
            goto failed;
        }
    }

    for ( ;; ) {

        /* cycle while we can write to a client */

        for ( ;; ) {

            rc = ngx_http_base64_filter_add_data(r, ctx);

            if (rc == NGX_DECLINED) {
                break;
            }

            rc = ngx_http_base64_filter_get_buf(r, ctx);

            if (rc == NGX_DECLINED) {
                break;
            }

            if (rc == NGX_ERROR) {
                goto failed;
            }

            rc = ngx_http_base64_filter_encode(r, ctx);

            if (rc == NGX_OK) {
                break;
            }

            if (rc == NGX_ERROR) {
                goto failed;
            }
        }

        if (ctx->out == NULL) {
            ngx_http_base64_filter_free_copy_buf(r, ctx);
            return ctx->busy ? NGX_AGAIN : NGX_OK;
        }

        rc = ngx_http_next_body_filter(r, ctx->out);

        if (rc == NGX_ERROR) {
            goto failed;
        }

        ngx_http_base64_filter_free_copy_buf(r, ctx);

        ngx_chain_update_chains(r->pool, &ctx->free, &ctx->busy, &ctx->out,
                                (ngx_buf_tag_t) &ngx_http_base64_filter_module);
        ctx->last_out = &ctx->out;

        if (ctx->done) {
            return rc;
        }
    }

failed:

    ctx->done = 1;

    ngx_http_base64_filter_free_copy_buf(r, ctx);

    return NGX_ERROR;
}


static ngx_int_t
ngx_http_base64_filter_encode_start(ngx_http_request_t *r,
                                    ngx_http_base64_ctx_t *ctx)
{
    ctx->started = 1;
    ctx->last_out = &ctx->out;
    ctx->flush = B64_NO_FLUSH;

    return NGX_OK;
}


static ngx_int_t
ngx_http_base64_filter_add_data(ngx_http_request_t *r, ngx_http_base64_ctx_t *ctx)
{
    if (ctx->b64stream.in.len || ctx->flush != B64_NO_FLUSH) {
        return NGX_OK;
    }

    if (ctx->in == NULL) {
        return NGX_DECLINED;
    }

    if (ctx->copy_buf) {
        ctx->copy_buf->next = ctx->copied;
        ctx->copied = ctx->copy_buf;
        ctx->copy_buf = NULL;
    }

    ctx->in_buf = ctx->in->buf;

    if (ctx->in_buf->tag == (ngx_buf_tag_t) &ngx_http_base64_filter_module) {
        ctx->copy_buf = ctx->in;
    }

    ctx->in = ctx->in->next;

    ctx->b64stream.in.data = ctx->in_buf->pos;
    ctx->b64stream.in.len = ctx->in_buf->last - ctx->in_buf->pos;

    if (ctx->in_buf->last_buf) {
        ctx->flush = B64_FINISH;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_base64_filter_get_buf(ngx_http_request_t *r, ngx_http_base64_ctx_t *ctx)
{
    int size = ctx->b64stream.in.len * 2;

    if (ctx->free) {
        ctx->out_buf = ctx->free->buf;
        ctx->free = ctx->free->next;
    } else {

        ctx->out_buf = ngx_create_temp_buf(r->pool, size);
        if (ctx->out_buf == NULL) {
            return NGX_ERROR;
        }

        ctx->out_buf->tag = (ngx_buf_tag_t) &ngx_http_base64_filter_module;
        ctx->out_buf->recycled = 1;
    }

    ctx->b64stream.out.data = ctx->out_buf->pos;
    ctx->b64stream.out.len = size;

    return NGX_OK;
}


static void
ngx_encode_base64_wrapper(ngx_str_t *dst, ngx_str_t *src, size_t *b64cnt, int flush)
{
    u_char         *d, *s;
    size_t          len;
    static u_char   basis64[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    len = src->len;
    s = src->data;
    d = dst->data;

    while (len > 2) {
        *d++ = basis64[(s[0] >> 2) & 0x3f];
        (*b64cnt)++;
        *d++ = basis64[((s[0] & 3) << 4) | (s[1] >> 4)];
        (*b64cnt)++;
        *d++ = basis64[((s[1] & 0x0f) << 2) | (s[2] >> 6)];
        (*b64cnt)++;
        *d++ = basis64[s[2] & 0x3f];
        (*b64cnt)++;

        if ((*b64cnt) == B64_F_LINE_LEN) {
            *d++ = '\r';
            *d++ = '\n';
            (*b64cnt) = 0;
        }

        s += 3;
        len -= 3;
    }

    if (len && flush == B64_FINISH) {
        *d++ = basis64[(s[0] >> 2) & 0x3f];
        (*b64cnt)++;

        if (len == 1) {
            *d++ = basis64[(s[0] & 3) << 4];
            (*b64cnt)++;
            *d++ = '=';
            (*b64cnt)++;

        } else {
            *d++ = basis64[((s[0] & 3) << 4) | (s[1] >> 4)];
            (*b64cnt)++;
            *d++ = basis64[(s[1] & 0x0f) << 2];
            (*b64cnt)++;
        }

        *d++ = '=';
        (*b64cnt)++;
    }

    if (flush == B64_FINISH && (*b64cnt) != 0) {
        *d++ = '\r';
        *d++ = '\n';
        (*b64cnt) = 0;
    }

    dst->len = d - dst->data;
}


static void
ngx_encode_base64_make(ngx_http_request_t *r, ngx_str_t *dst, ngx_str_t *src,
                       int flush, u_char *b64end, size_t *b64end_n, size_t *b64cnt)
{
    ngx_str_t tmpsrc;
    tmpsrc.data = (u_char*) ngx_pcalloc(r->pool, src->len + 2);

    if ((*b64end_n) == 1) {

        tmpsrc.data[0] = b64end[0];
        ngx_memcpy(tmpsrc.data + 1, src->data, src->len);
        tmpsrc.len = src->len + 1;

    } else if ((*b64end_n) == 2) {

        tmpsrc.data[0] = b64end[0];
        tmpsrc.data[1] = b64end[1];
        ngx_memcpy(tmpsrc.data + 2, src->data, src->len);

        tmpsrc.len = src->len + 2;
    } else {

        ngx_memcpy(tmpsrc.data, src->data, src->len);

        tmpsrc.len = src->len;
    }

    if (flush != B64_FINISH) {
        int flag = tmpsrc.len % 3;

        if (flag == 1) {
            b64end[0] = tmpsrc.data[tmpsrc.len - 1];
            tmpsrc.len -= 1;
            (*b64end_n) = 1;
        } else if (flag == 2) {
            b64end[1] = tmpsrc.data[tmpsrc.len - 1];
            b64end[0] = tmpsrc.data[tmpsrc.len - 2];
            tmpsrc.len -= 2;
            (*b64end_n) = 2;
        } else {
            (*b64end_n) = 0;
        }

    } else {
        (*b64end_n) = 0;
    }

    ngx_encode_base64_wrapper(dst, &tmpsrc, b64cnt, flush);

    ngx_pfree(r->pool, tmpsrc.data);
    tmpsrc.data = NULL;
    tmpsrc.len = 0;
}


static ngx_int_t
ngx_http_base64_filter_encode(ngx_http_request_t *r, ngx_http_base64_ctx_t *ctx)
{
    ngx_chain_t *cl;

    ngx_encode_base64_make(r, &ctx->b64stream.out, &ctx->b64stream.in, ctx->flush,
                           ctx->b64end, &ctx->b64end_n, &ctx->b64cnt);

    if (ctx->b64stream.in.data) {
        ctx->in_buf->pos = ctx->b64stream.in.data + ctx->b64stream.in.len;
        ctx->in_buf->last = ctx->in_buf->pos;

        ctx->b64stream.in.len = 0;
        ctx->b64stream.in.data = NULL;
    }

    ctx->out_buf->last = ctx->b64stream.out.data + ctx->b64stream.out.len;

    cl = ngx_alloc_chain_link(r->pool);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    if (ctx->flush == B64_FINISH) {

        ctx->done = 1;
        ctx->out_buf->last_buf = 1;

        cl->buf = ctx->out_buf;
        cl->next = NULL;
        *ctx->last_out = cl;
        ctx->last_out = &cl->next;

        ctx->b64stream.in.len = 0;
        ctx->b64stream.out.len = 0;

        return NGX_OK;

    } else {

        ctx->out_buf->last_buf = 0;

        cl->buf = ctx->out_buf;
        cl->next = NULL;
        *ctx->last_out = cl;
        ctx->last_out = &cl->next;
    }

    return NGX_AGAIN;
}


static void
ngx_http_base64_filter_free_copy_buf(ngx_http_request_t *r,
    ngx_http_base64_ctx_t *ctx)
{
    ngx_chain_t *cl;

    for (cl = ctx->copied; cl; cl = cl->next) {
        ngx_pfree(r->pool, cl->buf->start);
    }

    ctx->copied = NULL;
}


static ngx_int_t
ngx_http_base64_filter_init(ngx_conf_t *cf)
{
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_base64_header_filter;

    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_base64_body_filter;

    return NGX_OK;
}


static void *
ngx_http_base64_create_conf(ngx_conf_t *cf)
{
    ngx_http_base64_conf_t *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_base64_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->enable = NGX_CONF_UNSET;
    conf->max_length = NGX_CONF_UNSET;

    return conf;
}


static char *
ngx_http_base64_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_base64_conf_t *prev = parent;
    ngx_http_base64_conf_t *conf = child;

    ngx_conf_merge_value(conf->enable, prev->enable, 0);
    ngx_conf_merge_value(conf->max_length, prev->max_length, 1024 * 1024); /* 1MB */

    return NGX_CONF_OK;
}
