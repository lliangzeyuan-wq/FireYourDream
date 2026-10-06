# epoll 三大核心函数（对应图上三个API）

epoll 是 Linux 独有的 IO 多路复用，用来替代 select/poll，高并发性能更强。 三个函数：

1. `epoll_create()`：创建epoll实例（创建“蜂巢”）
2. `epoll_ctl()`：往epoll里**增/删/改**要监听的fd（往蜂巢放蜜蜂、拿走蜜蜂）
3. `epoll_wait()`：阻塞等待事件发生，只返回**有事件就绪的fd**（只拿已经采好蜜的蜜蜂）

> 类比你图里的**蜂巢**：
> 
> - epoll对象 = 蜂巢
> - fd（socket）= 蜜蜂
> - 事件POLLIN（可读）= 蜜蜂采蜜回来
> - poll/select：每次全部扫一遍所有蜜蜂，挨个检查有没有回来；
> - epoll：蜜蜂回来主动通知蜂巢，epoll_wait只拿到已经就绪的蜜蜂，不用遍历全部。

## 1. epoll_create

```
int epoll_create(int size);
```

- 作用：在内核创建一个epoll上下文，返回一个**epollfd**（文件描述符，代表这个蜂巢）
- 参数size：旧版本用来提示内核预估fd数量（大小确定），**现在已经废弃，填大于0就行**(现在变成类似链表的，不用预估)
- 返回值：成功返回epollfd；失败返回-1。
    
    > 一句话：**造一个空蜂巢**
    

> 新版本推荐 `epoll_create1(0)`，效果一样。

## 2. epoll_ctl（控制函数，最核心）

```
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
```

- epfd：epoll_create返回的epollfd（蜂巢）
- op：操作类型，3种：
    - `EPOLL_CTL_ADD`：新增fd，加入epoll监听（放蜜蜂进蜂巢）
    - `EPOLL_CTL_MOD`：修改这个fd监听的事件（修改蜜蜂关注的事件）
    - `EPOLL_CTL_DEL`：把fd从epoll移除（拿出蜜蜂）
- fd：要监听的文件描述符（socket）
- event：结构体，指定要监听什么事件，以及存用户数据

```c
struct epoll_event
{
    uint32_t events;  // EPOLLIN可读 / EPOLLOUT可写 / EPOLLET边缘触发 / EPOLLERR错误 / EPOLLHUP挂断
    epoll_data_t data;
};
```

struct epoll_event  中的 epoll_data_t data; 
```c
typedef union epoll_data
{
    void        *ptr;
    int          fd;
    uint32_t     u32;
    uint64_t     u64;
} epoll_data_t;
```
**int fd;（最常用！你写 epoll 服务器基本都用这个）** 保存你要监听的文件描述符（socket fd）。



`events = EPOLLIN` 代表监听**可读事件**（有新连接、客户端发数据）

> 一句话：**对蜂巢里面的蜜蜂做增删改**

## 3. epoll_wait（等待事件）

```
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
```

- epfd：蜂巢fd
- events：**输出数组**，内核把【已经就绪的fd事件】填到这个数组里
- maxevents：最多一次返回多少个就绪事件
- timeout：超时，-1代表永久阻塞
- 返回值nready：**有多少个fd就绪**（就是有多少蜜蜂回来了），0超时，-1出错

> 一句话：**等待，拿到就绪事件，只返回就绪的fd，不用遍历全部fd**

# epoll 和 poll/select 的核心区别（考试重点）

1. select/poll：每次调用都要把fd集合**从用户态拷贝到内核**；内核遍历全部fd，扫描有没有事件；返回后用户代码还要循环遍历所有fd判断。O(n)
2. epoll：
    - 注册时只拷贝一次fd信息；
    - 内核用回调机制，fd就绪主动放进就绪链表；
    - epoll_wait直接拿就绪链表，返回**只有就绪的fd**。O(1)
        
        > 所以上万并发连接epoll性能远高于poll、select。
        

# epoll 两种工作模式

1. LT 水平触发（默认）：只要缓冲区还有数据没读完，epoll_wait**反复触发事件**。简单，和poll行为类似。
2. ET 边缘触发：**只有状态发生变化那一刻触发一次**。读完数据才不会反复通知，性能更高，必须一次性读完缓冲区所有数据，代码更容易出bug。

# 最简epoll服务器骨架（和你前面poll代码对应）

```
int epfd = epoll_create(1);
struct epoll_event ev;
ev.events = EPOLLIN;
ev.data.fd = sockfd;
epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev); // 把监听socket加入epoll

while(1)
{
    struct epoll_event events[1024];
    int nready = epoll_wait(epfd, events, 1024, -1);
    for(int i=0; i<nready; i++)
    {
        int fd = events[i].data.fd;
        if(fd == sockfd)
        {
            // 有新连接，accept拿到clientfd，epoll_ctl加入epoll
        }
        else
        {
            // 客户端fd可读，recv读数据
        }
    }
}
```

# 面试简答背诵版

epoll 是Linux IO多路复用技术，提供3个API：

1. epoll_create 创建epoll对象，返回epollfd；
2. epoll_ctl，对fd执行添加、删除、修改监听事件；
3. epoll_wait阻塞等待就绪事件，返回就绪fd集合。 epoll基于内核回调，只返回就绪fd，适合高并发。

要不要我直接把**完整可编译epoll回声服务器代码**写出来，替换你刚才poll那版，方便直接对比poll和epoll写法差异？