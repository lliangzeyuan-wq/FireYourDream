#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <pthread.h>
#include <poll.h>




void * client_thread(void * arg)
{
    int clientfd = * (int *)

    arg;

    while (1)
    {

        char buffer[128] =
        {
            0
        };


        int count = recv(clientfd, buffer, 128, 0);

        if (count == 0)
        {
            break;
        }

        send(clientfd, buffer, count, 0);

        printf("clientfd: %d , count: %d , buffer: %s\n", clientfd, count, buffer);

    }

    close(clientfd);
}




// tcp
int main()
{

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(struct sockaddr_in));

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(2048);

    if (-1 == bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(struct sockaddr)))
    {
        perror("bind");
        return - 1;
    }

    listen(sockfd, 10);


#if 0
    struct sockaddr_in clientaddr;
    socklen_t len = sizeof(clientaddr);
    int clientfd = accept(sockfd, (struct sockaddr *) &clientaddr, &len);

    printf("accept\n");




#if 0
    char buffer[128] =
    {
        0
    };
    int count = recv(clientfd, buffer, 128, 0);

    send(clientfd, buffer, count, 0);


    printf("sockfd: %d, clientfd: %d , count: %d , buffer: %s\n", sockfd, clientfd, count, buffer);


#else



    while (1)
    {

        char buffer[128] =
        {
            0
        };


        int count = recv(clientfd, buffer, 128, 0);

        if (count == 0)
        {
            break;
        }

        send(clientfd, buffer, count, 0);

        printf("sockfd: %d, clientfd: %d , count: %d , buffer: %s\n", sockfd, clientfd, count, buffer);
    }


#endif

#elif 0

    while (1)
    {
        struct sockaddr_in clientaddr;
        socklen_t len = sizeof(clientaddr);
        int clientfd = accept(sockfd, (struct sockaddr *) &clientaddr, &len);

        pthread_t thid;

        //pthread_create：创建一个新的线程
        //&thid (pthread_t) : 用来存放新创建出来的线程ID (thid 是上面定义的一个变量名)
        //NULL : 线程属性，填 NULL 代表使用默认属性(栈大小、优先级全部默认）
        //client_thread : 线程入口函数名字
        //&clientfd : 传给线程入口函数的参数
        pthread_create(&thid, NULL, client_thread, &clientfd);
    }

#elif 0 //select

    fd_set rfds, rset;

    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);

    int maxfd = sockfd;

    printf("loop\n");

    while (1)
    {
        rset = rfds;
        int nready = select(maxfd + 1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(sockfd, &rset))
        {

            struct sockaddr_in clientaddr;
            socklen_t len = sizeof(clientaddr);
            int clientfd = accept(sockfd, (struct sockaddr *) &clientaddr, &len);

            printf("sockfd: %d , clientfd: %d\n", sockfd, clientfd);

            //每次accept新的连接的时候，调用下面两行代码，让select来监视
            FD_SET(clientfd, &rfds);
            maxfd = clientfd;
        }

        int i = 0;

        for (i = sockfd + 1; i <= maxfd; ++i)
        {
            if (FD_ISSET(i, &rset))
            {
                char buffer[128] =
                {
                    0
                };


                int count = recv(i, buffer, 128, 0);

                if (count == 0)
                {
                    printf("disconnect\n");

                    //两者都要有，做到一一对应，这样才是合格的断开
                    close(i);
                    FD_CLR(i , &rfds);


                    break;
                }

                send(i, buffer, count, 0);

                printf("clientfd: %d , count: %d , buffer: %s\n", i, count, buffer);

            }
        }

    }


#else


    struct pollfd fds[1024] =
    {
        0
    };


    //数组下标可以从0开始，和事件的fd保持一直是方便理解
    fds[sockfd].fd = sockfd;
    fds[sockfd].events = POLLIN;

    int maxfd = sockfd;

    while (1)
    {
        int nready = poll(fds, maxfd + 1, -1);

        if (fds[sockfd].revents & POLLIN)
        {

            struct sockaddr_in clientaddr;
            socklen_t len = sizeof(clientaddr);
            int clientfd = accept(sockfd, (struct sockaddr *) &clientaddr, &len);

            printf("sockfd: %d , clientfd: %d\n", sockfd, clientfd);

            fds[clientfd].fd = clientfd;
            fds[clientfd].events = POLLIN;

            maxfd = clientfd;
        }

        int i = 0;

        for (i = sockfd + 1; i <= maxfd; ++i)
        {
            if ( fds[i].revents & POLLIN)
            {
                char buffer[128] =
                {
                    0
                };


                int count = recv(i, buffer, 128, 0);

                if (count == 0)
                {
                    printf("disconnect\n");

                    //两者都要有，做到一一对应，这样才是合格的断开
                    close(i);

                    fds[i].fd = -1;
                    fds[i].events = 0;
                    break;
                }

                send(i, buffer, count, 0);

                printf("clientfd: %d , count: %d , buffer: %s\n", i, count, buffer);

            }

        }
    }


#endif






    //作用 ： 防止程序直接退出
    getchar();

    //close(clientfd);
}