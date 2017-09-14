#include <ngx_config.h>  
#include <ngx_core.h>  
#include <ngx_http.h>  

//模块关注的配置项
typedef struct {
	ngx_http_upstream_conf_t 		upstream;
} ngx_http_upstream_mytest_conf_t;

//上下文共享结构体
typedef struct {
	ngx_http_status_t               status;
	ngx_str_t						backendServer;
} ngx_http_upstream_mytest_ctx_t;


static ngx_int_t ngx_http_mytest_upstream_handler(ngx_http_request_t *r);
static char *ngx_http_mytest_upstream(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char* ngx_http_upstream_mytest_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static void* ngx_http_upstream_mytest_create_loc_conf(ngx_conf_t *cf);
static ngx_int_t mytest_upstream_process_header(ngx_http_request_t *r);

static ngx_str_t  ngx_http_proxy_hide_headers[] =
{
    ngx_string("Date"),
    ngx_string("Server"),
    ngx_string("X-Pad"),
    ngx_string("X-Accel-Expires"),
    ngx_string("X-Accel-Redirect"),
    ngx_string("X-Accel-Limit-Rate"),
    ngx_string("X-Accel-Buffering"),
    ngx_string("X-Accel-Charset"),
    ngx_null_string
};

static ngx_command_t ngx_http_upstream_mytest_module_commands[] = {  
    {  
        ngx_string("mytest"),  
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_HTTP_LMT_CONF | NGX_CONF_NOARGS, 
        ngx_http_mytest_upstream,  
        NGX_HTTP_LOC_CONF_OFFSET,  
        0,  
        NULL  
    },  
    ngx_null_command      
}; 

//定义模块初始化参数解析回调函数 暂时不处理
static ngx_http_module_t ngx_http_upstream_mytest_module_ctx = {  
    NULL,  
    NULL,  
    NULL,  
    NULL,  
    NULL,  
    NULL,  
    ngx_http_upstream_mytest_create_loc_conf,  
    ngx_http_upstream_mytest_merge_loc_conf
}; 

ngx_module_t ngx_http_upstream_mytest_module = {  
    NGX_MODULE_V1,  
    &ngx_http_upstream_mytest_module_ctx,  
    ngx_http_upstream_mytest_module_commands,  
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

static void* ngx_http_upstream_mytest_create_loc_conf(ngx_conf_t *cf)
{
	ngx_http_upstream_mytest_conf_t *mycf;

	mycf = (ngx_http_upstream_mytest_conf_t *)ngx_pcalloc(cf->pool, sizeof(ngx_http_upstream_mytest_conf_t));
	if (mycf == NULL) {
		return NULL;
	}

	mycf->upstream.connect_timeout  = 6000;
	mycf->upstream.send_timeout = 6000;
	mycf->upstream.read_timeout = 6000;
	mycf->upstream.store_access = 0600;

	//以固定大小转发响应包
	mycf->upstream.buffering = 0; 
	mycf->upstream.bufs.num = 8;
	mycf->upstream.bufs.size = ngx_pagesize;
	mycf->upstream.buffer_size = ngx_pagesize;
	mycf->upstream.busy_buffers_size  = 2*ngx_pagesize;
	mycf->upstream.temp_file_write_size = 2*ngx_pagesize;
	mycf->upstream.max_temp_file_size = 1024*1024*1024;

	mycf->upstream.hide_headers = NGX_CONF_UNSET_PTR;
	mycf->upstream.pass_headers = NGX_CONF_UNSET_PTR;
  printf(" %d %s debug\n", __LINE__, __FUNCTION__);
	return mycf;
}

static char* ngx_http_upstream_mytest_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
	ngx_http_upstream_mytest_conf_t *prev = (ngx_http_upstream_mytest_conf_t *)parent;
	ngx_http_upstream_mytest_conf_t *conf = (ngx_http_upstream_mytest_conf_t *)child;

	ngx_hash_init_t hash;
	hash.max_size = 100;
	hash.bucket_size = 1024;
	hash.name = "proxy_headers_hash";

	if (ngx_http_upstream_hide_headers_hash(cf, &conf->upstream, 
			&prev->upstream, ngx_http_proxy_hide_headers, &hash) != NGX_OK) {
		return NGX_CONF_ERROR;
	}
	return NGX_OK;
}

static ngx_int_t mytest_upstream_create_request(ngx_http_request_t *r)
{
	//模拟发往谷歌的请求
	static ngx_str_t backendQueryLine =
        ngx_string("GET /search?q=%V HTTP/1.1\r\nHost: www.sina.com\r\nConnection: close\r\n\r\n");
    ngx_int_t queryLineLen = backendQueryLine.len + r->args.len - 2;

    //在内存中申请内存，发送请求为异步操作
    ngx_buf_t *b = ngx_create_temp_buf(r->pool, queryLineLen);
    if (b == NULL) {
    	return NGX_ERROR;
    }
    b->last = b->pos + queryLineLen;
    ngx_snprintf(b->pos, queryLineLen, (char*)backendQueryLine.data, &r->args);

    r->upstream->request_bufs = ngx_alloc_chain_link(r->pool);
    if (r->upstream->request_bufs == NULL) {
    	return NGX_ERROR;
    }

    printf(" %d %s22222222222\n", __LINE__, __FUNCTION__);
    r->upstream->request_bufs->buf = b;
    r->upstream->request_bufs->next = NULL;

    r->upstream->request_sent = 0;
    r->upstream->header_sent = 0;

    r->header_hash = 1;

    return NGX_OK;
}

static ngx_int_t
mytest_process_status_line(ngx_http_request_t *r) 
{
	size_t	len;
	ngx_int_t              rc;
	ngx_http_upstream_t *u;

	ngx_http_upstream_mytest_ctx_t* ctx = ngx_http_get_module_ctx(r, ngx_http_upstream_mytest_module);
	if (ctx == NULL) {
		return NGX_ERROR;
	}

	u = r->upstream;
	rc = ngx_http_parse_status_line(r, &u->buffer, &ctx->status);
	if (rc == NGX_AGAIN) {
		return rc;
	}
	printf(" %d %s debug \n", __LINE__, __FUNCTION__);
	if (rc == NGX_ERROR) {
		ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
					"upstream sent no vaild HTTP/1.0 header");
		r->http_version = NGX_HTTP_VERSION_9;
		u->state->status = NGX_HTTP_OK;

		return NGX_OK;
	}
	/*以下表示在解析到完整的HTTP响应行时，会做一些简单得赋值操作，将解析出来的信息设置到
	r->upstream->headers_in结构中， 当upstream解析完所有的包头时，会把headers_in中的成员设置到
	将要向下游发送的r->headers_out结构体中，因为upstream希望能够按照ngx_http_upstream_conf_t配
	置结构体中的hide_headers等成员对发往下游的响应头部做统一处理*/

	if (u->state) {
		u->state->status = ctx->status.code;
	}
	u->headers_in.status_n = ctx->status.code;
	len = ctx->status.end - ctx->status.start;
	u->headers_in.status_line.len = len;

	u->headers_in.status_line.data = ngx_palloc(r->pool, len);
	if (u->headers_in.status_line.data == NULL) {
		return NGX_ERROR;
	}
	ngx_memcpy(u->headers_in.status_line.data, ctx->status.start, len);
	/*下一步开始解析HTTP头部， 设置process_header回调方法为mytest_upstream_process_heeader,之后
	再收到新字符流将由mytest_upstream_process_header解析*/
	u->process_header = mytest_upstream_process_header;

	return mytest_upstream_process_header(r);
}

static ngx_int_t 
mytest_upstream_process_header(ngx_http_request_t *r) 
{
	ngx_int_t                     rc;
	ngx_table_elt_t               *h;
	ngx_http_upstream_header_t    *hh;
	ngx_http_upstream_main_conf_t *umcf;
printf(" %d %s debug\n", __LINE__, __FUNCTION__);
	/*这里将upstram模块匹配项 ngx_http_upstream_main_conf_t取出来， 目的只有一个就是对将要转发给下游客户端
	的HTTP想用头部进行统一处理， 该结构体中存储了需要进行统一处理的HTTP头部名称和回调方法*/
	umcf = ngx_http_get_module_main_conf(r, ngx_http_upstream_module);
	for (;;) {
		rc = ngx_http_parse_header_line(r, &r->upstream->buffer, 1);
		if (rc == NGX_OK) {
			h = ngx_list_push(&r->upstream->headers_in.headers);
			if (h == NULL) {
				return NGX_ERROR;
			}
			h->hash = r->header_hash;
			h->key.len = r->header_name_end - r->header_name_start;
			h->value.len = r->header_end - r->header_start;
			h->key.data = ngx_palloc(r->pool, h->key.len + 1 + h->value.len + 1 + h->key.len);
			if (h->key.data  == NULL) {
				return NGX_ERROR;
			}
			h->value.data  = h->key.data + h->key.len + 1;
			h->lowcase_key = h->key.data + h->key.len + 1 + h->value.len + 1;
			h->key.data[h->key.len] = '\0';
			h->value.data[h->value.len] = '\0';

			if (h->key.len == r->lowcase_index) {
				ngx_memcpy(h->lowcase_key, r->lowcase_header,h->key.len);
			} else {
				ngx_strlow(h->lowcase_key, h->key.data, h->key.len);
			}

			//upstream模块会对HTTP头做一些特殊处理
			hh = ngx_hash_find(&umcf->headers_in_hash, h->hash,
			                   h->lowcase_key, h->key.len);
			if (hh && hh->handler(r, h, hh->offset) != NGX_OK) {
				return NGX_ERROR;
			}

			continue;
		}
		printf(" %d %s debug\n", __LINE__, __FUNCTION__);

		/*NGX_HTTP_PARSE_HEADER_DONE 表示HTTP头部解析完毕， 接下来接收到的都将是HTTP包体*/
		if (rc == NGX_HTTP_PARSE_HEADER_DONE) {
			if (r->upstream->headers_in.server == NULL) {
				h = ngx_list_push(&r->upstream->headers_in.headers);
				if (h == NULL) {
					return NGX_ERROR;
				}
				h->hash = ngx_hash(ngx_hash(ngx_hash(ngx_hash(
                                                         ngx_hash('s', 'e'), 'r'), 'v'), 'e'), 'r');
				ngx_str_set(&h->key, "Server");
				ngx_str_null(&h->value);
				h->lowcase_key = (u_char *) "server";
			}

			if (r->upstream->headers_in.date == NULL) {
				h = ngx_list_push(&r->upstream->headers_in.headers);
				if (h == NULL) {
					return NGX_ERROR;
				}
				h->hash = ngx_hash(ngx_hash(ngx_hash('d', 'a'), 't'), 'e');
				ngx_str_set(&h->key, "Date");
				ngx_str_null(&h->value);
				h->lowcase_key = (u_char *) "date";
			}
			return NGX_OK;
		}
		if (rc == NGX_AGAIN) {
			return NGX_AGAIN;
		}
printf(" %d %s debug\n", __LINE__, __FUNCTION__);
		ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
			"upstream send invalid header");
		return NGX_HTTP_UPSTREAM_INVALID_HEADER;
		/*mytest_upstream_process_header 返回OK后， upstream模块开始把上游的包体（如果有的话）直接
		转发到下游客户端*/
	}
}
static void 
mytest_upstream_finalize_request(ngx_http_request_t *r, ngx_int_t rc) 
{
	ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
			"mytest_upstream_finalize_request");
	printf(" %d %s debug\n", __LINE__, __FUNCTION__);
}

static ngx_int_t 
ngx_http_mytest_upstream_handler(ngx_http_request_t *r) 
{
	ngx_http_upstream_mytest_ctx_t *myctx = ngx_http_get_module_ctx(r, ngx_http_upstream_mytest_module);
	if (myctx == NULL) {
		myctx = ngx_palloc(r->pool, sizeof(ngx_http_upstream_mytest_ctx_t));
		if (myctx == NULL) {
			return NGX_ERROR;
		}
		ngx_http_set_ctx(r, myctx, ngx_http_upstream_mytest_module);
	}

	if (ngx_http_upstream_create(r) != NGX_OK) {
		ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
		             "ngx_http_upstream_create failed");
		return NGX_ERROR;
	}

	//得到配置文件
	ngx_http_upstream_mytest_conf_t *mycf = (ngx_http_upstream_mytest_conf_t *) ngx_http_get_module_loc_conf(
								r, ngx_http_upstream_mytest_module);
	ngx_http_upstream_t *u = r->upstream;
	u->conf = &mycf->upstream;
	u->buffering = mycf->upstream.buffering;

	u->resolved = (ngx_http_upstream_resolved_t *)ngx_pcalloc(r->pool, sizeof(ngx_http_upstream_resolved_t));
	if (u->resolved == NULL) {
		ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
					"ngx_pcalloc resolved error. %s", strerror(errno));
		return NGX_ERROR;
	}

	static struct sockaddr_in backendSockAddr;
	struct hostent *pHost = gethostbyname((char*)"www.sina.com");
	if (pHost == NULL) {
		ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
					"gethostbyname  failed. %s", strerror(errno));
		return NGX_ERROR;
	}
	backendSockAddr.sin_family = AF_INET;
	backendSockAddr.sin_port = htons((in_port_t)80);

	char* pDmsIP = inet_ntoa(*(struct in_addr*)(pHost->h_addr_list[0]));
	backendSockAddr.sin_addr.s_addr = inet_addr(pDmsIP);
	myctx->backendServer.data = (u_char*)pDmsIP;
	myctx->backendServer.len = strlen(pDmsIP);

	 ngx_log_debugall(r->connection->log, 0, "yang test ############################MYTEST upstream gethostbyname OK, addr:%s\n", pDmsIP);

	u->resolved->sockaddr = (struct sockaddr*)&backendSockAddr;
	u->resolved->socklen = sizeof(struct sockaddr_in);
	u->resolved->naddrs = 1;

	u->create_request = mytest_upstream_create_request;
	u->process_header = mytest_process_status_line;
	u->finalize_request = mytest_upstream_finalize_request;

	r->main->count++;
	ngx_http_upstream_init(r);

	printf(" %d %s debug\n", __LINE__, __FUNCTION__);
	return NGX_DONE;
}

static char *
ngx_http_mytest_upstream(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t  *clcf;
    
    //首先找到mytest配置项所属的配置块，clcf貌似是location块内的数据
//结构，其实不然，它可以是main、srv或者loc级别配置项，也就是说在每个
//http{}和server{}内也都有一个ngx_http_core_loc_conf_t结构体
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

    //http框架在处理用户请求进行到NGX_HTTP_CONTENT_PHASE阶段时，如果
//请求的主机域名、URI与mytest配置项所在的配置块相匹配，就将调用我们
//实现的ngx_http_mytest_handler方法处理这个请求
    clcf->handler = ngx_http_mytest_upstream_handler;

    return NGX_CONF_OK;
}