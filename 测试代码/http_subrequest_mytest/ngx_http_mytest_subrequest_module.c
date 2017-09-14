#include <ngx_config.h>  
#include <ngx_core.h>  
#include <ngx_http.h>  

typedef struct {
	ngx_str_t stock[6];
} ngx_http_mytest_ctx_t;


static char *ngx_http_mytest_subrequest(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static ngx_command_t ngx_http_subrequest_mytest_module_commands[] = {  
    {  
        ngx_string("mytest"),  
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_HTTP_LMT_CONF | NGX_CONF_NOARGS, 
        ngx_http_mytest_subrequest,  
        NGX_HTTP_LOC_CONF_OFFSET,  
        0,  
        NULL  
    },  
    ngx_null_command      
}; 

//定义模块初始化参数解析回调函数 暂时不处理
static ngx_http_module_t ngx_http_subrequest_mytest_module_ctx = {  
    NULL,  
    NULL,  
    NULL,  
    NULL,  
    NULL,  
    NULL,  
    NULL,  
    NULL
}; 

ngx_module_t ngx_http_mytest_subrequest_module = {  
    NGX_MODULE_V1,  
    &ngx_http_subrequest_mytest_module_ctx,  
    ngx_http_subrequest_mytest_module_commands,  
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

static void  mytest_post_handler(ngx_http_request_t *r)
{
	printf(" %d %s debug\n", __LINE__, __FUNCTION__);
	if (r->headers_out.status != NGX_HTTP_OK) 
	{
		printf(" %d %s debug %ld\n", __LINE__, __FUNCTION__, r->headers_out.status);
		ngx_http_finalize_request(r, r->headers_out.status);
		return;
	}

	ngx_http_mytest_ctx_t *myctx = ngx_http_get_module_ctx(r,
			                                ngx_http_mytest_subrequest_module);

	ngx_str_t output_format = ngx_string("stock[%V],Today current price:%V,volum:%V");
	ngx_int_t bodylen = output_format.len + myctx->stock[0].len 
		                + myctx->stock[1].len + myctx->stock[4].len -6;
	r->headers_out.content_length_n = bodylen;

	ngx_buf_t* b = ngx_create_temp_buf(r->pool, bodylen);
	ngx_snprintf(b->pos, bodylen, (char*)output_format.data, &myctx->stock[0],
	            &myctx->stock[1], &myctx->stock[4]);
	b->last = b->pos + bodylen;
	b->last_buf = 1;
	printf(" %d %s debug %s\n", __LINE__, __FUNCTION__, b->pos);

	ngx_chain_t out;
	out.buf = b;
	out.next = NULL;

	static ngx_str_t type = ngx_string("text/plain; charset=GBK");
	r->headers_out.content_type = type;
	r->headers_out.status = NGX_HTTP_OK;

	r->connection->buffered |= NGX_HTTP_WRITE_BUFFERED;
	ngx_int_t ret = ngx_http_send_header(r);
	ret = ngx_http_output_filter(r, &out);

	ngx_http_finalize_request(r, ret);
}


//子请求结束时需要处理
//1、获取子请求数据，保存在上下文
static ngx_int_t mytest_subrequest_post_handler(ngx_http_request_t *r, 
	                                            void *data, ngx_int_t rc)
{
	printf(" %d %s debug\n", __LINE__, __FUNCTION__);
	//当前请求时子请求
	ngx_http_request_t *pr = r->parent;
	ngx_http_mytest_ctx_t *myctx = ngx_http_get_module_ctx(pr,
			                                ngx_http_mytest_subrequest_module);

	pr->headers_out.status = r->headers_out.status;
	if (r->headers_out.status == NGX_HTTP_OK)
	{
		int flag = 0;
		ngx_buf_t* recv_buf = &r->upstream->buffer;
		for (; recv_buf->pos != recv_buf->last; recv_buf->pos++)
		{
			if (*recv_buf->pos == ',' || *recv_buf->pos =='\"')
			{
				if (flag > 0)
				{
					myctx->stock[flag-1].len = recv_buf->pos - myctx->stock[flag-1].data;
				}
				flag ++;
				myctx->stock[flag-1].data = recv_buf->pos + 1;
			}
			if (flag > 6)
			{
				break;
			}
		}
	}
	pr->write_event_handler = mytest_post_handler;

	return NGX_OK;
}

static ngx_int_t ngx_http_mytest_subrequest_handler(ngx_http_request_t *r)
{
	printf(" %d %s debug\n", __LINE__, __FUNCTION__);
	ngx_http_mytest_ctx_t *myctx = ngx_http_get_module_ctx(r,
			                                ngx_http_mytest_subrequest_module);
	if (myctx == NULL) 
	{
		myctx = ngx_pcalloc(r->pool, sizeof(ngx_http_mytest_ctx_t));
		if (myctx == NULL)
		{
			return NGX_ERROR;
		}
		ngx_http_set_ctx(r, myctx, ngx_http_mytest_subrequest_module);
	}

	ngx_http_post_subrequest_t *psr = ngx_palloc(r->pool, sizeof(ngx_http_post_subrequest_t));
	if (psr == NULL) {
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}

	psr->handler = mytest_subrequest_post_handler;
	psr->data = myctx;

	ngx_str_t sub_prefix = ngx_string("/list=");
	ngx_str_t sub_location;
	sub_location.len = sub_prefix.len + r->args.len;
	sub_location.data = ngx_palloc(r->pool, sub_location.len);
	ngx_snprintf(sub_location.data, sub_location.len, "%V%V", &sub_prefix, &r->args);

	ngx_http_request_t *sr;
	ngx_int_t rc = ngx_http_subrequest(r, &sub_location, NULL, &sr, psr, NGX_HTTP_SUBREQUEST_IN_MEMORY);
	if (rc != NGX_OK) {
		return NGX_ERROR;
	}
	printf(" %d %s debug\n", __LINE__, __FUNCTION__);
	return NGX_DONE;
}

static char *
ngx_http_mytest_subrequest(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t  *clcf;
    
    //首先找到mytest配置项所属的配置块，clcf貌似是location块内的数据
//结构，其实不然，它可以是main、srv或者loc级别配置项，也就是说在每个
//http{}和server{}内也都有一个ngx_http_core_loc_conf_t结构体
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

    //http框架在处理用户请求进行到NGX_HTTP_CONTENT_PHASE阶段时，如果
//请求的主机域名、URI与mytest配置项所在的配置块相匹配，就将调用我们
//实现的ngx_http_mytest_handler方法处理这个请求
    clcf->handler = ngx_http_mytest_subrequest_handler;

    return NGX_CONF_OK;
}

