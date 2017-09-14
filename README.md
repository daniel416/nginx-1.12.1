# reading-code-of-nginx-1.9.2  
nginx-1.9.2代码理解及详细注释  
 
   
说明:  
===================================   
nginx的以下功能模块的相关代码已经阅读，并对其源码及相关数据结构进行了详细备注，主要参考书籍为淘宝陶辉先生
的《深入理解Nginx:模块开发与架构解析》，并对书中没有讲到的相关部分功能进行了扩展，通过边阅读边调试的方法
通读了以下功能模块，并对其进行了详细的注释，同时加了自己的理解在里面，注释比较详细，由于是周末和下班时间阅读，再加上自己
文采限制，代码及数据结构注释都比较白话，注释中肯定会有理解错误的地方。  
  
  
      
阅读过程  
===================================  
	2017.9.11 通过资料了解nginx基本概况，使用情况，运行机制，并编写自己第一个http测试模块，后面了解具体过程。
	2017.9.12 进一步加深ngxin模块调用过程，nginx基本数据结构、上下文结构、配置项(nginx不同配置项解析函数)、日志文件。//TODO：编写解析配置项实例
	2017.9.13 熟悉nginx代理调用流程，并通过编写http_upstream_mytest模块实习upstream调用原理和基本接口的调用。

	2017.9.14 熟悉ngxin HTTP过滤模块流程 和 subrequest访问第三方模块如何处理返回http包体。并通过编写两个模块测试例子
			熟悉ngxin http模块的编写。




说明
===================================  
	gdb调试：
		1、修改nginx源代码目录下子目录/auto/cc中的conf文件，将ngx_compile_opt="-c"改为ngx_compile_opt="-c -g"
		2、编译时加上--with-debug 
		3、daemon          off; 
	 
     
编译方法：
===================================    
步骤1：这里根据需要编译自己的模块  
cd nginx-1.9.2  
./configure --add-module=./src/mytest_config --add-module=./src/my_test_module --add-module=./src/mytest_subrequest --add-module=./src/mytest_upstream --add-module=./src/ngx_http_myfilter_module --with-debug --with-file-aio --add-module=./src/sendfile_test --with-threads  --add-module=./src/nginx-requestkey-module-master/ --with-http_secure_link_module --add-module=./src/redis2-nginx-module-master/ 

 步骤2：make && make install  
