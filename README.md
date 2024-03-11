# 简介  
基于gperftools提供较高效的CPU、MEM Profile的解析。  
主要能力：  
- 提供profile IO
- 带可见符号profile的本地生成 
该数据后续可用于二次处理，比如火焰图的生成，可以不依赖服务二进制程序；  
- admin handler
在框架admin handler基础上提供对外的快速数据采集和火焰图生成能力。  
# 依赖 
依赖linux系统的binutils库，trpc官方镜像有提供，如果没有，需要手动安装。  
```shell
yum install binutils-devel.x86_64
```