![[Pasted image 20261006195105.png]]


- 客户端发两次大小为1000的包 ， 这两个包发送的时候间隔时间很短
- 服务端recv的时候一次最多能recv1500个大小 ， 因此第一次recv就recv了1500 = 1000 + 500   ， 然后再recv 500。

- 服务端recv的时候把第一个包和第二个包的一部分混在一起接受了



# 解决方案

## 1 .  自定义包头   -    LT水平触发

![[Pasted image 20261006195513.png]]

- 在发的每个包的前面加上长度

```
// 定义2字节变量，用来存放网络传来的数据包长度（包头）
short length = 0;
// 第一步：读取包头，约定包头固定占2字节，保存到length变量
// fd：socket描述符；&length：接收缓冲区；2：想要读取2字节；0：默认阻塞recv
recv(fd, &length, 2, 0);

// ntohs：network to host short
// 将网络大端序的2字节长度，转换为本机CPU的小端序数值
length = ntohs(length);
// 第二步：根据包头得到的长度，读取后面的业务数据到buffer
recv(fd, buffer, length, 0);
```


