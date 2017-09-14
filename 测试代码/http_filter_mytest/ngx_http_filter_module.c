#include <ngx_config.h>  
#include <ngx_core.h>  
#include <ngx_http.h>  

typedef struct {
	ngx_flag_t enable;
} ngx_http_myfilter_conf_t;

typedef struct {
	ngx_int_t add_pregix;
} ngx_http_myfilter_ctx_t;
static ngx_str_t filter_prefix = ngx_string("[my filter prefix]");

static void* ngx_http_myfilter_create_loc_conf(ngx_conf_t *cf);
static char* ngx_http_myfilter_merge_loc_conf(ngx_conf_t *cf, void *parent, void* child);
static ngx_int_t ngx_http_myfilter_init(ngx_conf_t *cf);

static ngx_command_t ngx_http_myfilter_commands[] = {  
    {  
        ngx_string("add_prefix"),  
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_HTTP_LMT_CONF | NGX_CONF_FLAG, 
        ngx_conf_set_flag_slot,  
        NGX_HTTP_LOC_CONF_OFFSET,  
        offsetof(ngx_http_myfilter_conf_t, enable),  
        NULL  
    },  
    ngx_null_command      
}; 

//定义模块初始化参数解析回调函数 暂时不处理
static ngx_http_module_t ngx_http_myfilter_ctx = {  
    NULL,  
    ngx_http_myfilter_init,  
    NULL,  
    NULL,  
    NULL,  
    NULL,  
    ngx_http_myfilter_create_loc_conf,  
    ngx_http_myfilter_merge_loc_conf
}; 

ngx_module_t ngx_http_filter_module = {  
    NGX_MODULE_V1,  
    &ngx_http_myfilter_ctx,  
    ngx_http_myfilter_commands,  
    NGX_HTTP_MODULE,  
    NULL,  
    NULL,  
    NULL,  
    NULL,  
    NULL,  
    NULL,  
    NULL,  
    NGX_MODULE_V1_PADDING  
};  

static void* ngx_http_myfilter_create_loc_conf(ngx_conf_t *cf)
{
	printf(" %d %s debug\n", __LINE__, __FUNCTION__);
	ngx_http_myfilter_conf_t *mycf;
	mycf = ngx_pcalloc(cf->pool, sizeof(ngx_http_myfilter_conf_t));
	if (mycf == NULL) {
		return NULL;
	}

	mycf->enable = NGX_CONF_UNSET;

	return mycf;
}

static char*
ngx_http_myfilter_merge_loc_conf(ngx_conf_t *cf, void *parent, void* child)
{
	printf(" %d %s debug\n", __LINE__, __FUNCTION__);
	ngx_http_myfilter_conf_t *prev = (ngx_http_myfilter_conf_t*)parent;
	ngx_http_myfilter_conf_t *conf = (ngx_http_myfilter_conf_t*)child;

	ngx_conf_merge_value(conf->enable, prev->enable, 0);

	return NGX_CONF_OK;	
}

static ngx_http_output_header_filter_pt ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt ngx_http_next_body_filter;

static ngx_int_t 
ngx_http_myfilter_header_filter(ngx_http_request_t *r)
{
	printf(" %d %s debug\n", __LINE__, __FUNCTION__);
	ngx_http_myfilter_ctx_t *ctx;
	ngx_http_myfilter_conf_t *conf;

	if (r->headers_out.status != NGX_HTTP_OK) {
		return ngx_http_next_header_filter(r);
	}
	printf(" %d %s debug\n", __LINE__, __FUNCTION__);
	ctx = ngx_http_get_module_ctx(r, ngx_http_filter_module);
	if (ctx) {
		return ngx_http_next_header_filter(r);
	}

	conf = ngx_http_get_module_loc_conf(r, ngx_http_filter_module);

	if (conf->enable == 0) {
		return ngx_http_next_header_filter(r);
	}

	ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_myfilter_ctx_t));
	if (ctx == NULL) {
		return NGX_ERROR;
	}
	ctx->add_pregix = 0;
	ngx_http_set_ctx(r, ctx, ngx_http_filter_module);

	if(r->headers_out.content_type.len >= sizeof("text/plain") - 1
		&& ngx_strncasecmp(r->headers_out.content_type.data, (u_char*)"text/plain"
			, sizeof("text/plain") - 1) == 0)
	{
		ctx->add_pregix = 1;
		if (r->headers_out.content_length_n > 0) {
			r->headers_out.content_length_n += filter_prefix.len;
		}
	}
	printf(" %d %s debug\n", __LINE__, __FUNCTION__);
	return ngx_http_next_header_filter(r);
}

static ngx_int_t 
ngx_http_myfilter_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
	printf(" %d %s debug\n", __LINE__, __FUNCTION__);
	ngx_http_myfilter_ctx_t *ctx;
	ctx = ngx_http_get_module_ctx(r, ngx_http_filter_module);
	if (ctx == NULL || ctx->add_pregix != 1) {
		return ngx_http_next_body_filter(r, in);
	}
printf(" %d %s debug\n", __LINE__, __FUNCTION__);
	ctx->add_pregix = 2;

	ngx_buf_t *b = ngx_create_temp_buf(r->pool, filter_prefix.len);
	//ngx_memcpy(b->pos, filter_prefix.data, filter_prefix.len);
	b->start = b->pos = filter_prefix.data;
	b->last = b->pos + filter_prefix.len;

	ngx_chain_t *cl = ngx_alloc_chain_link(r->pool);
	cl->buf = b;
	cl->next = in;

	return ngx_http_next_body_filter(r, cl);
}


static ngx_int_t ngx_http_myfilter_init(ngx_conf_t *cf)
{
	ngx_http_next_header_filter = ngx_http_top_header_filter;
	ngx_http_top_header_filter = ngx_http_myfilter_header_filter;

	ngx_http_next_body_filter = ngx_http_top_body_filter;
	ngx_http_top_body_filter = ngx_http_myfilter_body_filter;

	return NGX_OK;
}
