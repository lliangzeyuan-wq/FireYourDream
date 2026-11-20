#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <getopt.h>

#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAX_MAS_LENGTH       512
#define TIME_SUB_MS(tv1, tv2) ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)

/* ===================== 单条命令 ===================== */

int send_msg(int connfd, char *msg, int length) {
    int res = send(connfd, msg, length, 0);
    if (res < 0) {
        perror("send");
        return -1;
    }
    return res;
}

int recv_msg(int connfd, char *msg, int length) {
    int res = recv(connfd, msg, length, 0);
    if (res < 0) {
        perror("recv");
        return -1;
    }
    return res;
}

int equals(const char *pattern, const char *result, const char *casename) {
    if (strcmp(pattern, result) == 0) {
        // printf("==> PASS --> %s\n", casename);
        return 0;
    }
    printf("==> FAILED --> %s, '%s' != '%s'\n", casename, pattern, result);
    return -1;
}

/* 返回：0 成功 / -1 结果不符 / -2 IO 出错 */
int test_case(int connfd, char *msg, char *pattern, char *casename) {
    if (!msg || !pattern || !casename) return -1;

    if (send_msg(connfd, msg, strlen(msg)) < 0) return -2;

    char result[MAX_MAS_LENGTH] = {0};
    int n = recv_msg(connfd, result, MAX_MAS_LENGTH - 1);
    if (n <= 0) return -2;

    return equals(pattern, result, casename);
}

/* 一轮 6 条命令；key 带上线程编号，避免多线程互相覆盖同一个 key */
int array_testcase(int connfd, int id) {
    int failed = 0, rc;
    char cmd[128];

    snprintf(cmd, sizeof(cmd), "SET Name%d King", id);
    rc = test_case(connfd, cmd, "SUCCESS", "SETCase");
    if (rc == -2) return -2; if (rc) failed++;

    snprintf(cmd, sizeof(cmd), "GET Name%d", id);
    rc = test_case(connfd, cmd, "King", "GETCase");
    if (rc == -2) return -2; if (rc) failed++;

    snprintf(cmd, sizeof(cmd), "MOD Name%d Darren", id);
    rc = test_case(connfd, cmd, "SUCCESS", "MODCase");
    if (rc == -2) return -2; if (rc) failed++;

    snprintf(cmd, sizeof(cmd), "GET Name%d", id);
    rc = test_case(connfd, cmd, "Darren", "GETCase");
    if (rc == -2) return -2; if (rc) failed++;

    snprintf(cmd, sizeof(cmd), "DEL Name%d", id);
    rc = test_case(connfd, cmd, "SUCCESS", "DELCase");
    if (rc == -2) return -2; if (rc) failed++;

    snprintf(cmd, sizeof(cmd), "GET Name%d", id);
    rc = test_case(connfd, cmd, "NO EXIST", "GETCase");
    if (rc == -2) return -2; if (rc) failed++;

    return failed;
}

int connect_tcpserver(const char *ip, unsigned short port) {

    int connfd = socket(AF_INET, SOCK_STREAM, 0);
    if (connfd < 0) { perror("socket"); return -1; }

    struct sockaddr_in tcpserver_addr;
    memset(&tcpserver_addr, 0, sizeof(struct sockaddr_in));

    tcpserver_addr.sin_family = AF_INET;
    tcpserver_addr.sin_addr.s_addr = inet_addr(ip);
    tcpserver_addr.sin_port = htons(port);

    int ret = connect(connfd, (struct sockaddr*)&tcpserver_addr, sizeof(struct sockaddr_in));
    if (ret) {
        close(connfd);      /* 不在这里打印：多线程会刷屏，由主线程汇总 */
        return -1;
    }

    return connfd;
}

/* ===================== 多线程压测 ===================== */

typedef struct {
    const char *ip;
    int port;
    int id;         /* 线程编号：用作独立 key 的后缀 */
    int count;      /* 本线程跑多少轮（每轮 6 条命令） */
    int failed;          /* 本线程失败的命令数 */
    int io_error;        /* 连接中断标记 */
    int connect_failed;  /* 本线程连接失败 */
    long done_rounds;    /* 本线程真正跑完的轮数 */
} thread_arg_t;

static void *test_entry(void *arg) {
    thread_arg_t *t = (thread_arg_t *)arg;

    int connfd = connect_tcpserver(t->ip, t->port);
    if (connfd < 0) {
        t->connect_failed = 1;      /* 连接失败单独统计，不算命令失败 */
        t->io_error = 1;
        return NULL;
    }

    for (int i = 0; i < t->count; i++) {
        int rc = array_testcase(connfd, t->id);
        if (rc == -2) { t->io_error = 1; break; }
        t->failed += rc;
        t->done_rounds++;           /* 只有完整跑完一轮才计数 */
    }

    close(connfd);
    return NULL;
}



// ./testcase -s 127.0.0.1 -p 2048 -m 1 -t 50 -n 100000 
// -t : 线程数
// -n : 跑的轮数(每轮6条命令)
int main(int argc, char *argv[]) {

    char ip[16] = {0};
    int port = 0;
    int mode = 1;
    int threadnum = 1;
    int total_count = 100000;   /* 总轮数：每轮 6 条命令 */

    int opt;
    while ((opt = getopt(argc, argv, "s:p:m:t:n:")) != -1) {

        switch (opt) {
            case 's':
                strcpy(ip, optarg);
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'm':
                mode = atoi(optarg);
                break;
            case 't':
                threadnum = atoi(optarg);
                break;
            case 'n':
                total_count = atoi(optarg);
                break;
            default:
                printf("Usage: %s -s <ip> -p <port> [-m mode] [-t threads] [-n total_rounds]\n", argv[0]);
                return -1;
        }
    }

    if (ip[0] == '\0' || port <= 0 || threadnum <= 0 || total_count <= 0) {
        printf("Usage: %s -s <ip> -p <port> [-m mode] [-t threads] [-n total_rounds]\n", argv[0]);
        return -1;
    }

    if (!(mode & 0x1)) return 0;

    thread_arg_t *args = calloc(threadnum, sizeof(thread_arg_t));
    pthread_t *tids = calloc(threadnum, sizeof(pthread_t));
    if (!args || !tids) { perror("calloc"); return -1; }

    int base = total_count / threadnum;
    int rem  = total_count % threadnum;
    int actual_rounds = 0;
    for (int i = 0; i < threadnum; i++) {
        args[i].ip = ip;
        args[i].port = port;
        args[i].id = i;
        args[i].count = base + (i < rem ? 1 : 0);
        actual_rounds += args[i].count;
    }

    struct timeval tv_begin, tv_end;
    gettimeofday(&tv_begin, NULL);

    for (int i = 0; i < threadnum; i++) {
        pthread_create(&tids[i], NULL, test_entry, &args[i]);
        usleep(1000);   /* 错开 1ms，避免 50 个连接同时 SYN 冲垮 listen backlog */
    }

    for (int i = 0; i < threadnum; i++) {
        pthread_join(tids[i], NULL);
    }

    gettimeofday(&tv_end, NULL);

    int failed = 0, io_error = 0, conn_fail = 0;
    long done_rounds = 0;
    for (int i = 0; i < threadnum; i++) {
        failed += args[i].failed;
        done_rounds += args[i].done_rounds;
        if (args[i].io_error) io_error = 1;
        if (args[i].connect_failed) conn_fail++;
    }

    int time_used = TIME_SUB_MS(tv_end, tv_begin);
    long planned   = (long)actual_rounds * 6;   /* 计划发送的请求数 */
    long completed = done_rounds * 6;           /* 实际完成的请求数 */

    if (conn_fail > 0) {
        /* 连不上服务器：一个请求都没发出去，QPS 没有意义 */
        printf("连接失败: %d/%d 个线程连不上 %s:%d\n", conn_fail, threadnum, ip, port);
        printf("planned: %ld requests, completed: %ld, time_used: %d ms —— QPS 无意义，不计算\n",
               planned, completed, time_used);
    } else if (failed > 0 || io_error) {
        /* 跑了一部分但中间出错：QPS 仅供参考 */
        printf("threads: %d, completed: %ld/%ld requests, failed: %d%s, time_used: %d ms —— 有失败，QPS 仅供参考\n",
               threadnum, completed, planned, failed,
               io_error ? " (有连接中断)" : "", time_used);
    } else {
        printf("threads: %d, requests: %ld, failed: 0, time_used: %d ms, qps: %.0f\n",
               threadnum, completed, time_used,
               time_used > 0 ? completed * 1000.0 / time_used : 0.0);
    }

    free(args);
    free(tids);
    return 0;
}
