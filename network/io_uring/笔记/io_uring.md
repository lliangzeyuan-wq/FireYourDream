把recv/read , 读写io改成异步
- 原来是app发送请求 ，等待内核返回值之后再继续往下面走
- 改为异步之后是发送多个请求 ， 一起返回

写代码时候的流程
1. init/create
2. commit
3. callback()
4. destroy()


内核层面：io_uring提供的3个系统调用的接口
1. io_uring_setup()  =>    init/create
2. io_uring_register()   =>   commit
3. io_uring_enter()     =>    callback()

应用层：开发者在3个系统调用之上开发的库liburing
1. io_uring_queue_init_params
	1. 初始化两个队列 ：sq 和 cq
2. io_uring_prep_accept
3. io_uring_prep_recv
4. io_uring_prep_send
5. io_uring_submit
	1. 将提交队列（SQ）中已准备好的请求（SQE）批量提交给内核执行。内核处理完成后，会将结果（CQE）放入完成队列（CQ）中。
6. io_uring_wait_cqe
	1. 阻塞当前线程，直到完成队列（CQ）中产生至少一个结果。若队列为空，线程进入睡眠状态；有任务完成时，内核唤醒线程并交出对应的完成事件（CQE）。
7. io_uring_peek_batch_cqe
	1. 以非阻塞的方式查看完成队列（CQ），一次性批量提取最多指定数量的完成事件。若当前无完成事件，则立即返回，适用于高并发场景下的批量处理，以减少系统调用开销。
8. io_uring_cq_advance
	1. 一次性移动完成队列（CQ）的头部索引 `count` 个位置。通常配合 `io_uring_peek_batch_cqe` 使用，用于批量确认结果已消费。其作用等同于连续调用多次 `io_uring_cqe_seen`，但性能更高。



测磁盘的iops(全称是 Input/Output Operations Per Second，翻译过来就是“每秒输入/输出操作次数”。)

- pofix.fio
```bash

[global]
thread=1
group_reporting=1
direct=1
verify=0
time_based=1
runtime=10
bs=16K
size=16384
iodepth=64
rw=randwrite
filename=kingfs
# 同步
ioengine=psync
# 异步
#ioengine=io_uring

[test]
stonewall
description="variable bs"
```

- `fio pofix.fio`   =>  能得出一些性能，其中就有iops

pofix.fio 中同步的时候 (  `ioengine=psync`  )   
![[Pasted image 20261111192933.png]]

pofix.fio 中异步的时候 (  `ioengine=io_uring`  )   
![[Pasted image 20261111193146.png]]

- iops的性能提升了4倍



io_uring工作流程

![[Pasted image 20261111210214.png]]

1. sq/sqe (Submission Queue Entry)
 应用程序（比如你的程序或 `fio`）想要干的活（比如“读取 16KB 数据”或“写一个网络包”）。程序把这些请求填写成 SQE，然后丢进共享内存的提交队列里。 (sq是一个队列 ， sqe是队列中的一个个的元素)
2. run
中间那个圈代表**内核（Kernel）**。内核通过轮询或中断机制，把 `sqe` 里的任务拿出来，真正在底层（比如硬盘或网卡）去执行（Run）这个 I/O 操作。这个阶段是异步的，程序不用傻等。
3. cq/cqe (Completion Queue Entry)
内核干完活后，把执行结果（成功还是失败、读到了多少数据）打包成 CQE，扔进共享内存的完成队列里。应用程序只需要去这个 `cqe` 圈里拿结果就行了。（cq是一个队列 ， cqe是队列中的一个个的元素）



用io_uring写代码的体验
1. 感觉不出异步