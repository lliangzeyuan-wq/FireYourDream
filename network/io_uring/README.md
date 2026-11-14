### 介绍
- io_uring 是 Linux 内核（5.1+）提供的一套异步 I/O 框架。
- 具体的看笔记
- 环境：linux

### pofix.fio 
这是 fio 的任务描述文件（job file），用来测试磁盘读写性能。

- 使用方法：
  ``` fio posix.fio```
(fio 是一个工具，需要先安装)
  

### vio_iouring.c
- 介绍：本代码是用liburing库提供的接口编写的TCP服务端demo（liburing是有在io_uring提供的接口上编写的一个应用层的库）
- 编译 ： gcc vio_iouring.c -o vio_iouring -luring

- 运行： ./vio_iouring

- 测试： 用网络连接工具连接9999这个端口，发送信息，服务端会打印客户端发送的信息