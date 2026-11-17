
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/time.h>
#include <pthread.h>
#include <arpa/inet.h>

typedef struct test_context_s {
    char serverip[16] ;
    int port;
    int threadnum ;
    int connection ;
    int requestion ; //总的要发送的请求数量

#if 1
    int failed;
#endif

} test_context_t;    




int connect_tcpserver(const char *ip, unsigned short port){

    int connfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in tcpserver_addr;
    memset(&tcpserver_addr, 0, sizeof(struct sockaddr_in));

    tcpserver_addr.sin_family = AF_INET;
    tcpserver_addr.sin_addr.s_addr = inet_addr(ip);
    tcpserver_addr.sin_port = htons(port);

    int ret = connect(connfd, (struct sockaddr*)&tcpserver_addr, sizeof(struct sockaddr_in));
    if(ret){
        perror("connect\n");
        return -1;
    }

    return connfd;
}    

// 64字节
#define TEST_MESSAGE        "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuvwxyz\r\n"
#define RBUFFER_LENGTH      2048
#define WBUFFER_LENGTH      2048


#define TIME_SUB_MS(tv1, tv2) ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)


int send_recv_tcppkt(int fd){

#if 1
    int res = send(fd, TEST_MESSAGE, strlen(TEST_MESSAGE), 0);
    if(res < 0){
        exit(1);
    }

    char rbuffer[RBUFFER_LENGTH] = {0};
    res = recv(fd, rbuffer, RBUFFER_LENGTH, 0);
    if(res <= 0){
        exit(1);
    }

    if(strcmp(rbuffer, TEST_MESSAGE) != 0){
        printf("failed : '%s' != '%s'\n", rbuffer, TEST_MESSAGE);
        return -1;
    }
#else 
    char wbuffer[WBUFFER_LENGTH] = {0};
    int i = 0;

    // 发送的包wbuffer的大小 ：(64 * 循环次数) 字节  
    //我测试的时候用的是  i<2  , i<4,i<8 (128,256,512 bytes)
    // i<17的时候会出现客户端发送和收到的不一样的情况，会出现报错刷屏
    //  ，出现这种情况的原因是字节数太大，把一个大的包切成几个小的包发送的情况
    for(i = 0; i<2; ++i){
        strcpy(wbuffer + i * strlen(TEST_MESSAGE), TEST_MESSAGE);
    }    

    int res = send(fd, wbuffer, strlen(wbuffer), 0);
    if(res < 0){
        exit(1);
    }

    char rbuffer[RBUFFER_LENGTH] = {0};
    res = recv(fd, rbuffer, RBUFFER_LENGTH, 0);
    if(res <= 0){
        exit(1);
    }

    if(strcmp(rbuffer, wbuffer) != 0){
        printf("failed : '%s' != '%s'\n", rbuffer, wbuffer );
        return -1;
    }
#endif
    return 0;
}



static void *test_qps_entry(void *arg){
    test_context_t *pctx = (test_context_t*)arg;

    // 1. 计算每个线程应该开几个连接
    int conns_per_thread = pctx->connection / pctx->threadnum;

    // 2. 计算每个连接需要发送几次请求 (总请求数 / 总连接数)
    int reqs_per_conn = pctx->requestion / pctx->connection;

     // 简单的容错，防止除数为0(应为除法是向下取整)
    if (conns_per_thread == 0) conns_per_thread = 1;
    if (reqs_per_conn == 0) reqs_per_conn = 1;

    
    for (int c = 0; c < conns_per_thread; c++) {
        int connfd = connect_tcpserver(pctx->serverip, pctx->port);
        if (connfd < 0) {
            printf("connect_tcpserver failed\n");
            continue;
        }

        for (int i = 0; i < reqs_per_conn; i++) {
            int res = send_recv_tcppkt(connfd);
            if (res != 0) {
                printf("send_recv_tcppkt failed\n");
                pctx->failed++;
                continue;
            }
        }

        close(connfd);
    }    
     
    return NULL;
}




// ./test_qps_tcpclient -s 127.0.0.1 -p 2048 -t 50 -c 100 -n 10000
int main(int argc, char *argv[]){

    test_context_t ctx = {0};
    

    int opt;
    while((opt = getopt(argc, argv, "s:p:t:c:n:")) != -1){

        switch (opt){
            case 's':
                printf("-s: %s\n", optarg);
                strcpy(ctx.serverip, optarg);
                break;
            case 'p':
                printf("-p: %s\n", optarg);
                ctx.port = atoi(optarg);
                break;
            case 't':
                printf("-t: %s\n", optarg);
                ctx.threadnum = atoi(optarg);
                break;
            case 'c':
                printf("-c: %s\n", optarg);
                ctx.connection = atoi(optarg);
                break;
            case 'n':
                printf("-n: %s\n", optarg);
                ctx.requestion = atoi(optarg);
                break;
            default:
                return -1;
        }
    }


    //pthread_t：线程 ID 的类型（pthread 库里面用来存线程编号）
    pthread_t *ptid = malloc(ctx.threadnum * sizeof(pthread_t));
    int i = 0;

    struct timeval tv_begin;
    gettimeofday(&tv_begin, NULL);
    for(i = 0; i<ctx.threadnum; ++i){
        pthread_create(&ptid[i], NULL, test_qps_entry, &ctx);
    }    
    for(i = 0; i<ctx.threadnum; ++i){
        //阻塞等待子线程结束 ， 
        //第二个参数 ：用来接收子线程结束时的返回值
        pthread_join(ptid[i], NULL);
    }    
    struct timeval tv_end;
    gettimeofday(&tv_end, NULL);

    int time_used = TIME_SUB_MS(tv_end, tv_begin);

    printf("success: %d, failed: %d, time_used: %d, qps: %d\n", ctx.requestion-ctx.failed, 
    ctx.failed, time_used, ctx.requestion * 1000/time_used);



    return 0;

}
