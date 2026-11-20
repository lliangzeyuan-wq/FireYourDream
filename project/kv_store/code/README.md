# 测试
## 先把服务端运行起来
```
$ make clean
$ make
$ ./kvstore
```

## 运行封装的测试代码
```
$ gcc testcase.c -o testcase

$ ./testcase -s 127.0.0.1 -p 2048 -m 1 -t 50 -n 100000 
```

- 解释：
  - -s : 连接的主机
  - -p : 服务器的端口
  - -m : 选择的数据结构的类型（1：数组 ，2 ：红黑树 ...）
  - -t : 创建的线程数
  - -n : 测试的轮数（一轮6个请求，因此共有 6*n 个请求）


## 一个测试结果

![image.png](https://lliangzeuyuan-1481077693.cos.ap-guangzhou.myqcloud.com/obsidian_img/20261120171544780.png)

- 输出结果解释：
  - threads : 开的线程数
  - requests ：成功的总的请求数
  - failed ： 失败的请求数
  - time_used : 总耗时
  - qps : 每秒处理的请求数