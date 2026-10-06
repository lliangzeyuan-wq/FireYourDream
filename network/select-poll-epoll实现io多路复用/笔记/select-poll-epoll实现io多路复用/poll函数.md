# poll 函数讲解

```
#include <poll.h>
int poll(struct pollfd *fds, nfds_t nfds, int timeout);
```

## 1. 三个参数

### 参数1：`struct pollfd *fds`

`fds` 是**结构体数组首地址**，数组里每一项代表一个要监听的文件描述符。 `struct pollfd` 结构体：

```
struct pollfd {
    int   fd;         // 要监听的文件描述符（socketfd/clientfd）
    short events;     // 【你告诉内核】我想监听这个fd的什么事件
    short revents;    // 【内核返回给你】这个fd实际发生了什么事件
};
```

常用事件(pollfd结构体中的short events)：

- `POLLIN`：可读事件（新连接到来 / 客户端发数据 / 客户端断开）
- `POLLOUT`：可写事件（缓冲区有空，可以send）
- `POLLERR`：出错（内核自动设置，不需要你在events里写）

> 注意：`events` 是**用户设置**；`revents` 是**poll返回后内核填充**，只读。

### 参数2：`nfds_t nfds`

数组里**需要检查的最大下标+1**（不是数组总长度）。 比如你监听fd最大是5，就填 `6`，poll只会遍历下标0~5。

> 对应你代码里：`poll(fds, maxfd+1, -1);`

### 参数3：`int timeout`

阻塞等待的超时时间，**单位：毫秒ms**

- `timeout = -1`：永久阻塞，直到有事件发生（和select传NULL一样）
- `timeout = 0`：不阻塞，立刻返回，轮询
- `timeout > 0`：最多等待timeout毫秒，超时后返回0

---

## 2. 返回值 `int nready`

```
int nready = poll(fds, maxfd+1, -1);
```

1. **nready > 0**：有事件就绪，返回值=**有事件的fd总个数**。 你需要遍历fds数组，检查每个元素的`revents`，判断哪个fd有事件。
2. **nready == 0**：超时了，没有任何fd就绪。
3. **nready < 0（-1）**：调用出错，`errno`保存错误原因。

> 和select一个巨大区别： ✅ poll**不会修改原来的events**，不需要每次循环重新拷贝fd集合； ❌ select会修改fd集合，每次循环必须 `rset = rfds`。

## 3. 结合你截图里的代码框架

```
struct pollfd fds[1024] = {0};
fds[sockfd].fd = sockfd;
fds[sockfd].events = POLLIN;  // 监听sockfd可读（新连接）
int maxfd = sockfd;

while (1) {
    int nready = poll(fds, maxfd+1, -1); // 阻塞等待事件

    // 遍历fds数组，检查revents
    for(int i = 0; i <= maxfd; i++)
    {
        if(fds[i].revents & POLLIN) // 这个fd可读
        {
            if(i == sockfd)
            {
                // sockfd可读：新连接，accept拿到clientfd
                // 把clientfd放进fds数组：
                fds[clientfd].fd = clientfd;
                fds[clientfd].events = POLLIN;
                if(clientfd > maxfd) maxfd = clientfd;
            }
            else
            {
                // 客户端fd可读，recv读数据
                int count = recv(i, buf, 128, 0);
                if(count == 0)
                {
                    // 客户端断开
                    close(i);
                    fds[i].fd = -1; // poll里fd=-1会自动跳过这个条目
                }
            }
        }
    }
}
```

> poll 断开处理小技巧：`fds[i].fd = -1`，下一次poll会直接忽略这个fd，不用像select那样FD_CLR。

## 4. poll vs select 简单对比

1. select：fd集合有最大上限（默认1024），每次调用会修改fd集合，每次循环要拷贝。
2. poll：用数组存fd，没有固定上限，**不会修改events**，更方便。

## 5. 易错点

- 判断事件要用**按位与`&`**：`if(fds[i].revents & POLLIN)`，不能用`==`
- `revents`是内核填充，不要手动修改
- 客户端断开的时候，`revents`同样会触发`POLLIN`，`recv`返回0，和select行为一致

