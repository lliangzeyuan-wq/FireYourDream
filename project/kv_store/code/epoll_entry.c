#include <signal.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <pthread.h>
#include <sys/poll.h>
#include <sys/epoll.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

#include"kvstore.h"



#define ENABLE_HTTP_RESPONSE 1






//listenfd 触发EPOLLIN事件的时候执行
int accept_cb(int fd);

//clientfd 触发EPOLLIN事件的时候执行
int recv_cb(int fd);

//clientfd 触发EPOLLOUT事件的时候执行
int send_cb(int fd);

//event
int set_event(int fd, int event, int flag);


int epfd = 0;

struct timeval tv_begin;

/*
//这个结构体太大了 ， 我先把他改小一点
struct conn_item connlist[1048576] =
{
    0
};   //这个数组大于了2GB的空间
*/

struct conn_item connlist[1024] =
{
    0
};   //这个数组大于了2GB的空间




#define TIME_SUB_MS(tv1, tv2) ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)


#if 0


struct reactor
{
    int epfd;
    struct conn_item * connlist;
};


#else

#endif


#if ENABLE_HTTP_RESPONSE

#define ROOT_DIR "/home/lzy/Share/0voice/doc/"

typedef struct conn_item connection_t;

int http_request(connection_t *conn)
{

}

int http_response(connection_t *conn) {
#if 1
    conn->wlen = sprintf(conn->wbuffer,
        "HTTP/1.1 200 OK\r\n"
        "Accept-Ranges: bytes\r\n"
        "Content-Length: 82\r\n"
        "Content-Type: text/html\r\n"
        "Date: Sat, 06 Aug 2022 13:16:46 GMT\r\n\r\n"
        "<html><head><title>0voice.king</title></head><body><h1>King</h1></body></html>\r\n\r\n");
#else

    int filefd = open(ROOT_DIR"index.html" , O_RDONLY);

    struct stat stat_buf;
    fstat(filefd, &stat_buf);
    
     conn->wlen = sprintf(conn->wbuffer,
        "HTTP/1.1 200 OK\r\n"
        "Accept-Ranges: bytes\r\n"
        "Content-Length: %ld\r\n"
        "Content-Type: text/html\r\n"
        "Date: Sat, 06 Aug 2022 13:16:46 GMT\r\n\r\n", stat_buf.st_size);
    
    int count = read(filefd, conn->wbuffer + conn->wlen, BUFFER_LENGTH-conn->wlen);
    conn->wlen += count;

#endif 
    return conn->wlen;
}


#endif 

int set_event(int fd, int event, int flag)
{
    if (flag)
    {
        struct epoll_event ev;
        ev.events = event;
        ev.data.fd = fd;
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    }
    else 
    {
        struct epoll_event ev;
        ev.events = event;
        ev.data.fd = fd;
        epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);

    }

}


int accept_cb(int fd)
{
    struct sockaddr_in clientaddr;
    socklen_t len = sizeof(clientaddr);
    int clientfd = accept(fd, (struct sockaddr *) &clientaddr, &len);

    if (clientfd < 0)
    {
        return - 1;
    }


    set_event(clientfd, EPOLLIN, 1);


    connlist[clientfd].fd = clientfd;
    memset(connlist[clientfd].rbuffer, 0, BUFFER_LENGTH);
    connlist[clientfd].rlen = 0;
    memset(connlist[clientfd].wbuffer, 0, BUFFER_LENGTH);
    connlist[clientfd].wlen = 0;
    connlist[clientfd].recv_t.recv_callback = recv_cb;
    connlist[clientfd].send_callback = send_cb;

    if((clientfd % 1000) == 999)
    {
        struct timeval tv_cur;
        gettimeofday(&tv_cur, NULL);
        int time_used = TIME_SUB_MS(tv_cur, tv_begin);

        //把tv_cur 的东西，拷贝覆盖到tv_begin ,  开始新一轮的计时
        memcpy(&tv_begin, &tv_cur, sizeof(struct timeval));
        
        printf("clientfd: %d, time_used: %d(ms)\n" , clientfd ,time_used);
    }
    
    return clientfd;
}


int recv_cb(int fd)
{
    char * buffer = connlist[fd].rbuffer;
    int idx = connlist[fd].rlen;


    int count = recv(fd, buffer + idx, BUFFER_LENGTH - idx, 0);

    if (count == 0)
    {
     //   printf("disconnect\n");

        //两者都要有，做到一一对应，这样才是合格的断开
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
        close(fd);
        return - 1;
    }

    buffer[idx + count] = '\0';

    connlist[fd].rlen = count;

//这里设置服务器回发什么数据
#if 0 // echo: need to send
    memcpy(connlist[fd].wbuffer, connlist[fd].rbuffer, connlist[fd].rlen);
    connlist[fd].wlen = connlist[fd].rlen;
#else

    kvstore_request(&connlist[fd]);
    connlist[fd].wlen = strlen(connlist[fd].wbuffer);  

#endif
   connlist[fd].rlen = 0;
    set_event(fd, EPOLLOUT, 0);


    return count;
}


int send_cb(int fd)
{
    char * buffer = connlist[fd].wbuffer;
    int idx = connlist[fd].wlen;

    int count = send(fd, buffer, idx, 0);

    //event
    set_event(fd, EPOLLIN, 0);

    return count;
}



int init_server(unsigned short port)
{

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);


    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(struct sockaddr_in));

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(port);

    if (-1 == bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(struct sockaddr)))
    {
        perror("bind");
        return - 1;
    }

    listen(sockfd, 1024);

    return sockfd;

}

#if 0
// tcp
int main()
{

    signal(SIGPIPE, SIG_IGN);

    int port_count = 20;
    unsigned short port = 2048;
    int i = 0;
    
    epfd = epoll_create(1);

    for(i = 0; i<port_count; i++)
    {
        int sockfd = init_server(port + i);
        connlist[sockfd].fd = sockfd;
        connlist[sockfd].recv_t.accept_callback = accept_cb;
        set_event(sockfd, EPOLL_CTL_ADD, 1);
    }


    gettimeofday(&tv_begin, NULL);
        

    struct epoll_event events[1024] =
    {
        0
    };


    while (1) //mainloop()
    {
        int nready = epoll_wait(epfd, events, 1024, -1);

        int i = 0;

        for (i = 0; i < nready; ++i)
        {
            int connfd = events[i].data.fd;
            if (events[i].events & EPOLLIN)
            {
                int count = connlist[connfd].recv_t.recv_callback(connfd);
                //printf("recv count: %d <-- buffer: %s\n", count, connlist[connfd].rbuffer);
            }
            else if (events[i].events & EPOLLOUT)
            {

               // printf("send --> buffer: %s\n", connlist[connfd].wbuffer);
                int count = connlist[connfd].send_callback(connfd);
               
            }
        }
    }







    //作用 ： 防止程序直接退出
    getchar();

    //close(clientfd);
}


#else

// tcp
int epoll_entry(void)
{

    signal(SIGPIPE, SIG_IGN);

    int port_count = 20;
    unsigned short port = 2048;
    int i = 0;
    
    epfd = epoll_create(1);

    for(i = 0; i<port_count; i++)
    {
        int sockfd = init_server(port + i);
        connlist[sockfd].fd = sockfd;
        connlist[sockfd].recv_t.accept_callback = accept_cb;
        set_event(sockfd, EPOLL_CTL_ADD, 1);
    }


    gettimeofday(&tv_begin, NULL);
        

    struct epoll_event events[1024] =
    {
        0
    };


    while (1) //mainloop()
    {
        int nready = epoll_wait(epfd, events, 1024, -1);

        int i = 0;

        for (i = 0; i < nready; ++i)
        {
            int connfd = events[i].data.fd;
            if (events[i].events & EPOLLIN)
            {
                int count = connlist[connfd].recv_t.recv_callback(connfd);
                //printf("recv count: %d <-- buffer: %s\n", count, connlist[connfd].rbuffer);
            }
            else if (events[i].events & EPOLLOUT)
            {

               // printf("send --> buffer: %s\n", connlist[connfd].wbuffer);
                int count = connlist[connfd].send_callback(connfd);
               
            }
        }
    }







    //作用 ： 防止程序直接退出
    //getchar();

    //close(clientfd);
}


#endif




