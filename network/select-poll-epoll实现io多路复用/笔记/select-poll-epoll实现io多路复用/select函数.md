# select 原型

```
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
```

- `select (maxfd , rset , wset , eset , timeout）`
	- maxfd ： 最大fd
	- rset ： read 设置
	- wset ： write 设置
	- eset ： error 设置 
	- timeout : 超时设置


- select 没活干的时候会阻塞住


## 缺陷
- fd_set  对应的结构体类型在内核里面写死了1024 ， 因此只能处理1024以内的连接数

# 第一个参数：**nfds（也常叫 maxfdp1）**

## 一句话结论

> **nfds = 三个fd集合(readfds/writefds/exceptfds)里面，最大的文件描述符数值 + 1**

## 原理（对应你图上画的0,1,2,3,4,5）

文件描述符是从 **0 开始编号**：

- 0：标准输入 stdin
- 1：标准输出 stdout
- 2：标准错误 stderr
- 3：你的监听socket sockfd
- 4、5……：后面accept得到的clientfd

内核拿到`nfds`之后，**只会遍历 0 ~ nfds-1 这些fd**，检查这些fd有没有事件就绪。

> 例子： 现在集合里最大fd是5，那`nfds = 5 + 1 = 6` 内核就循环检查 fd=0,1,2,3,4,5，一共6个。

为什么要`+1`？因为遍历区间是**左闭右开 [0, nfds)**。

）


1. **每次新增/删除fd，要重新计算更新nfds** 有新客户端连接，clientfd变大，nfds必须跟着更新；否则内核不会去检查新的大fd。

## 和你前面“一线程一连接”对比

- 一连接一线程：每个连接单独开线程，靠操作系统调度线程；C10K时线程太多，资源爆炸。
- select：**单个线程，同时监视一堆fd**，只在fd就绪时去处理，这就是IO多路复用。

## 补充面试考点

- select有上限：`FD_SETSIZE` 默认一般1024，也就是最多只能监视小于1024的fd，这是select的硬缺陷，后面引出poll、epoll。
- readfds/writefds/exceptfds 是**输入输出参数**，select返回后集合会被修改，每次循环调用select都要重新`FD_SET`。



# 第二个参数：`fd_set *readfds`

## 含义

`readfds` 是**读事件文件描述符集合**，用来告诉内核：**我想监听哪些fd，等待它们“可读”事件**。

> 可读事件常见场景：
> 
> 1. socket收到客户端发来数据（recv能读到数据）
> 2. 对端关闭连接（recv返回0）
> 3. 管道、文件有数据可读

`fd_set` 本质是一个**位图（bitmask）**，每一位代表一个fd：

- 第N位 = 1：代表fd=N 加入监听集合，等待可读事件
- 第N位 = 0：不监听这个fd

## ==配套4个宏（操作fd_set）==

```
FD_ZERO(fd_set *set);    // 清空整个集合，所有bit置0，每次循环开头必调用
FD_SET(int fd, fd_set *set);  // 把fd加入集合，对应bit置1
FD_CLR(int fd, fd_set *set);  // 把fd从集合移除，对应bit置0
FD_ISSET(int fd, fd_set *set); // select返回后，判断该fd是否就绪（bit被内核置1）
```

## ⚠️ 非常关键特性（面试高频坑）

**readfds 是【输入输出参数】**

- 调用select前：你设置哪些fd要监听可读；
- select返回的时候：**内核会修改这个集合**，只保留发生可读事件的fd，其余全部清0。 👉 所以：**每一轮循环调用select之前，必须重新 FD_ZERO + FD_SET 重新装填fd集合！**

## 补充

1. 如果**不需要监听任何可读事件**，第二个参数直接填 `NULL`；
2. 区分：
    - `readfds`：读事件（收数据、新连接）
    - `writefds`：第三个参数，写事件（缓冲区有空，可以send）
    - `exceptfds`：第四个参数，异常事件（带外数据）

## 面试一句话总结

> `readfds` 是可读fd集合，调用前由用户设置要监听哪些fd；select返回后集合被内核改写，仅保留就绪fd；每次循环调用select前必须重新构建fd集合。





# select 第三个参数：`fd_set *writefds`

## 作用

`writefds`：**可写事件的fd集合**。 你把需要监听“可写就绪”的文件描述符放进这个集合，让内核监视它们。

> 什么叫可写就绪：调用`send/write`不会被阻塞。 常见触发场景：

1. socket的**内核发送缓冲区还有剩余空间**，可以往里面写数据；
2. 非阻塞 `connect` 完成（连接成功时，socket会触发写就绪）；
3. 管道写端可以写入。

## 使用方式

和`readfds`完全一样，用这4个宏操作位图：

- `FD_ZERO(&wset);` 清空集合
- `FD_SET(fd, &wset);` 将fd加入写监听集合
- `FD_CLR(fd, &wset);` 从集合移除fd
- `FD_ISSET(fd, &wset);` select返回后判断该fd是否可写就绪

## 核心坑（面试必考）

`writefds` 是**输入输出参数**

1. 调用select前：你设置哪些fd要监听写事件；
2. select返回后：**内核会修改这个集合**，只保留已经就绪的fd，其余bit清零。
    
    > 👉 每一轮循环调用select，**都要重新 FD_ZERO + FD_SET 填充writefds**。
    

## 区分记忆

- readfds（第2参数）：**对方发数据过来 / 新连接到达**（读就绪）
- writefds（第3参数）：**内核发送缓冲区有空，我可以向外发数据**（写就绪）

一句话总结：`writefds` 是监听fd什么时候可以写而不阻塞；不需要监听写事件就填NULL；每次循环必须重新构造集合。



# 第四个参数：`fd_set *exceptfds`
## 含义

`exceptfds`：**异常事件fd集合**。 用来监听文件描述符上发生**异常/带外数据**。 同样是 `fd_set` 位图，使用同一套 `FD_ZERO / FD_SET / FD_CLR / FD_ISSET` 宏。

> 最常用场景：**socket 收到 TCP 带外数据（OOB data）** TCP带外数据：紧急数据，优先传输，不排队在普通接收缓冲区。

> 注意：不是普通的连接断开、读写错误！ 普通断开、普通读写错误，**不会触发exceptfds**，而是在readfds里触发可读事件，recv返回0或者-1。

## 核心特性

`exceptfds` 同样是**输入输出参数**

- 调用select前：你把想要监听异常事件的fd放进集合；
- select返回后：内核修改集合，只保留发生异常事件的fd； 👉 每一轮循环select前，都要重新装填这个集合。

## 三个集合快速对比

- readfds（第2参）：可读事件（收到普通数据、新连接、对方关闭）
- writefds（第3参）：可写事件（发送缓冲区有空，可send）
- exceptfds（第4参）：异常事件（TCP带外数据OOB）

## 面试一句话总结

> exceptfds 是用来监听fd的异常事件，主要用于捕获TCP带外数据；普通TCP服务器基本不用，传NULL；同样每次循环需要重新构造集合。




# 第五个参数：`struct timeval *timeout`


## struct timeval 结构体

```
struct timeval {
    long tv_sec;   // 秒
    long tv_usec;  // 微秒，1s = 1000000 us
};
```

## 三种取值情况（重点，面试高频）

### 情况1：timeout = NULL

`select(..., NULL);`

- **永久阻塞**，一直等，直到有fd就绪（读/写/异常事件）或者被信号打断才返回。
- 没有事件就一直卡在select函数里面。

### 情况2：timeout 指向结构体，并且 `tv_sec=0, tv_usec=0`

```
struct timeval tv = {0, 0};
select(..., &tv);
```

- **非阻塞轮询**：不等待。
- 立刻检查一遍所有fd，马上返回结果，不管有没有就绪事件。
    
    > 适合：不停循环快速轮询，但是非常耗CPU。
    

### 情况3：timeout 指向结构体，设置大于0的时间

```
struct timeval tv = {3, 500000}; // 3秒 + 500000微秒 = 3.5秒
select(..., &tv);
```

- **带超时的阻塞等待**：最多等待指定时长。
    1. 在超时时间内有fd就绪 → 立刻返回；
    2. 时间到了，没有任何fd就绪 → select返回0。

> ⚠️ 重要坑： Linux下，**select返回的时候，timeout的值会被内核修改**，变成**剩余还没等完的时间**。 所以如果要固定每次都等待3.5秒，**每次循环调用select前，都要重新给tv_sec、tv_usec赋值！**


## 一句话面试总结

> timeout用来设置select的阻塞等待时长。传NULL代表无限阻塞；{0,0}代表不阻塞，立刻轮询；设置正数代表最长等待时间；注意Linux中select会修改timeval，每次循环调用前必须重新赋值。




# 返回值

- `>0`：有事件就绪，返回就绪fd的总个数；
- `=0`：超时了，没有任何fd就绪；
- `<0`：出错（被信号中断、参数错误等）。


