#include <ngx_config.h>  
#include <ngx_core.h>  
#include <ngx_http.h>  


static ngx_int_t ngx_http_first_handler(ngx_http_request_t *r)  
{  
    printf("lcl test:xxxxxxxxx <%s, %u>\n",  __FUNCTION__, __LINE__);
    // Only handle GET/HEAD method  ////必须是GET或者HEAD方法，否则返回405 Not Allowed
    if (!(r->method & (NGX_HTTP_GET | NGX_HTTP_HEAD))) {  
        return NGX_HTTP_NOT_ALLOWED;  
    }  

    // Discard request body  
    ngx_int_t rc = ngx_http_discard_request_body(r);  
    //如果不想处理请求中的包体，那么可以调用ngx_http_discard_request_body方法将接收自客户端的HTTP包体丢弃掉。
    if (rc != NGX_OK) {  
        return rc;  
    }  
  
    // Send response header  
    ngx_str_t type = ngx_string("text/plain");  
    ngx_str_t response = ngx_string("Hello World  first!!!11111111111");  
    r->headers_out.status = NGX_HTTP_OK;  
    r->headers_out.content_length_n = response.len;  
    r->headers_out.content_type = type;  

    /*
    请求处理完毕后，需要向用户发送HTTP响应，告知客户端Nginx的执行结果。HTTP响应主要包括响应行、响应头部、包体三部分。
    发送HTTP响应时需要执行发送HTTP头部（发送HTTP头部时也会发送响应行）和发送HTTP包体两步操作。
    */
    rc = ngx_http_send_header(r);  
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {  
        return rc;  
    }  
    // Send response body  
    ngx_buf_t *b;  
    b = ngx_create_temp_buf(r->pool, response.len);  
    if (b == NULL) {  
        return NGX_HTTP_INTERNAL_SERVER_ERROR;  
    }  
    ngx_memcpy(b->pos, response.data, response.len);  
    b->last = b->pos + response.len;  
    b->last_buf = 1;  
  
    ngx_chain_t out;  
    out.buf = b;  
    out.next = NULL;  

    return ngx_http_output_filter(r, &out);  
}  


static char* ngx_http_first(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)  
{  
    ngx_http_core_loc_conf_t *clcf;  

    /*找到配置块， clcf看上去像location块内的数据结构， 它可以试main srv或者loc级别配置项*/
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);  

    clcf->handler = ngx_http_first_handler;   //HTTP框架在接收完HTTP请求的头部后，会调用handler指向的方法

    return NGX_CONF_OK;  
}  
//设置first配置项的回调函数
static ngx_command_t ngx_http_first_commands[] = {  
    {  
        ngx_string("first"),  
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_HTTP_LMT_CONF | NGX_CONF_NOARGS, 
        ngx_http_first,  
        NGX_HTTP_LOC_CONF_OFFSET,  
        0,  
        NULL  
    },  
    ngx_null_command      
}; 

//定义模块初始化参数解析回调函数 暂时不处理
static ngx_http_module_t ngx_http_first_module_ctx = {  
    NULL,  
    NULL,  
    NULL,  
    NULL,  
    NULL,  
    NULL,  
    NULL,  
    NULL  
}; 

//定义模块
ngx_module_t ngx_http_first_module = {  
    NGX_MODULE_V1,  
    &ngx_http_first_module_ctx,  
    ngx_http_first_commands,  
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
