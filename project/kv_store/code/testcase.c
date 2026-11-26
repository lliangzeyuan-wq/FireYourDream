/*
 * testcase.c 
 *
 * mode 表示使用哪种数据结构（引擎）或测试模式：
 *   -m 1 : 数组引擎     SET  / GET  / MOD  / DEL
 *   -m 2 : 红黑树引擎   RSET / RGET / RMOD / RDEL
 *   -m 3 : 两种都跑（每轮 12 条命令）
 *   -m 4 : 红黑树节点计数测试（RCOUNT，必须 -t 1）
 *
 * 其它参数：
 *   -s <ip>        服务器地址
 *   -p <port>      服务器端口
 *   -t <threads>   线程数（每个线程一条连接；mode 4 必须为 1）
 *   -n <rounds>    总轮数（所有线程加起来；mode 4 时为节点个数）
 *
 * 示例：
 *   ./testcase -s 127.0.0.1 -p 9096 -m 1 -t 10 -n 10000   # 数组
 *   ./testcase -s 127.0.0.1 -p 9096 -m 2 -t 10 -n 10000   # 红黑树
 *   ./testcase -s 127.0.0.1 -p 9096 -m 3 -t 10 -n 10000   # 两种都跑
 *   ./testcase -s 127.0.0.1 -p 9096 -m 4 -t 1  -n 1000    # 红黑树节点计数测试（必须 -t 1）
 */

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

/* ===================== 数组引擎：SET/GET/MOD/DEL ===================== */

int array_testcase(int connfd, int id) {
    int failed = 0, rc;
    char cmd[128];

    snprintf(cmd, sizeof(cmd), "SET Name%d King", id);
    rc = test_case(connfd, cmd, "SUCCESS", "SETCase");
    if (rc == -2) return -2;
    if (rc) failed++;

    snprintf(cmd, sizeof(cmd), "GET Name%d", id);
    rc = test_case(connfd, cmd, "King", "GETCase");
    if (rc == -2) return -2;
    if (rc) failed++;

    snprintf(cmd, sizeof(cmd), "MOD Name%d Darren", id);
    rc = test_case(connfd, cmd, "SUCCESS", "MODCase");
    if (rc == -2) return -2;
    if (rc) failed++;

    snprintf(cmd, sizeof(cmd), "GET Name%d", id);
    rc = test_case(connfd, cmd, "Darren", "GETCase");
    if (rc == -2) return -2;
    if (rc) failed++;

    snprintf(cmd, sizeof(cmd), "DEL Name%d", id);
    rc = test_case(connfd, cmd, "SUCCESS", "DELCase");
    if (rc == -2) return -2;
    if (rc) failed++;

    snprintf(cmd, sizeof(cmd), "GET Name%d", id);
    rc = test_case(connfd, cmd, "NO EXIST", "GETCase");
    if (rc == -2) return -2;
    if (rc) failed++;

    return failed;
}

/* ===================== 红黑树引擎：RSET/RGET/RMOD/RDEL ===================== */

int rbtree_testcase(int connfd, int id) {
    int failed = 0, rc;
    char cmd[128];

    snprintf(cmd, sizeof(cmd), "RSET RName%d King", id);
    rc = test_case(connfd, cmd, "SUCCESS", "RSETCase");
    if (rc == -2) return -2;
    if (rc) failed++;

    snprintf(cmd, sizeof(cmd), "RGET RName%d", id);
    rc = test_case(connfd, cmd, "King", "RGETCase");
    if (rc == -2) return -2;
    if (rc) failed++;

    snprintf(cmd, sizeof(cmd), "RMOD RName%d Darren", id);
    rc = test_case(connfd, cmd, "SUCCESS", "RMODCase");
    if (rc == -2) return -2;
    if (rc) failed++;

    snprintf(cmd, sizeof(cmd), "RGET RName%d", id);
    rc = test_case(connfd, cmd, "Darren", "RGETCase");
    if (rc == -2) return -2;
    if (rc) failed++;

    snprintf(cmd, sizeof(cmd), "RDEL RName%d", id);
    rc = test_case(connfd, cmd, "SUCCESS", "RDELCase");
    if (rc == -2) return -2;
    if (rc) failed++;

    snprintf(cmd, sizeof(cmd), "RGET RName%d", id);
    rc = test_case(connfd, cmd, "NO EXIST", "RGETCase");
    if (rc == -2) return -2;
    if (rc) failed++;

    return failed;
}

/* ---------- 读取一条命令的回复（用于 RCOUNT 这种需要读数值的场景） ---------- */
static int query_value(int connfd, const char *cmd, char *buf, int len) {

    if (send_msg(connfd, (char *)cmd, strlen(cmd)) < 0) return -1;

    memset(buf, 0, len);
    int n = recv_msg(connfd, buf, len - 1);
    if (n <= 0) return -1;

    return 0;
}

/* ---------- mode 4：红黑树节点计数测试 ----------
 * 插入一个节点，RCOUNT 应 +1；删除一个节点，RCOUNT 应 -1。
 * 注意：RCOUNT 是全局计数，必须单线程（-t 1）运行。
 */
int rbtree_count_test(int connfd, int count) {

    int failed = 0;
    char cmd[128], expect[64], actual[128];
    int base = 0;

    /* 先读基线，避免服务端已有残留节点导致期望值错位 */
    if (query_value(connfd, "RCOUNT", actual, sizeof(actual)) < 0) return -2;
    base = atoi(actual);
    printf("起始 RCOUNT = %d, 计划插入 %d 个节点\n", base, count);

    /* ---------- 插入阶段：每插一个，RCOUNT +1 ---------- */
    for (int i = 0; i < count; i++) {

        snprintf(cmd, sizeof(cmd), "RSET CountName%d King%d", i, i);

        int rc = test_case(connfd, cmd, "SUCCESS", "RSET");
        if (rc == -2) return -2;
        if (rc) failed++;

        if (query_value(connfd, "RCOUNT", actual, sizeof(actual)) < 0) return -2;

        snprintf(expect, sizeof(expect), "%d", base + i + 1);
        if (equals(expect, actual, "RCOUNT-after-RSET")) failed++;
    }

    /* ---------- 删除阶段：每删一个，RCOUNT -1 ---------- */
    for (int i = 0; i < count; i++) {

        snprintf(cmd, sizeof(cmd), "RDEL CountName%d", i);

        int rc = test_case(connfd, cmd, "SUCCESS", "RDEL");
        if (rc == -2) return -2;
        if (rc) failed++;

        if (query_value(connfd, "RCOUNT", actual, sizeof(actual)) < 0) return -2;

        snprintf(expect, sizeof(expect), "%d", base + count - (i + 1));
        if (equals(expect, actual, "RCOUNT-after-RDEL")) failed++;
    }

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
        close(connfd);      /* 不逐线程打印，由主线程汇总 */
        return -1;
    }

    return connfd;
}

/* ===================== 多线程压测 ===================== */

typedef struct {
    const char *ip;
    int port;
    int mode;       /* 1=数组  2=红黑树  3=两者都跑 */
    int id;         /* 线程编号：用作独立 key 的后缀 */
    int count;      /* 本线程跑多少轮 */
    int failed;          /* 本线程失败的命令数 */
    int io_error;        /* 连接中断标记 */
    int connect_failed;  /* 本线程连接失败 */
    long done_rounds;    /* 本线程真正跑完的轮数 */
} thread_arg_t;

static void *test_entry(void *arg) {
    thread_arg_t *t = (thread_arg_t *)arg;

    int connfd = connect_tcpserver(t->ip, t->port);
    if (connfd < 0) {
        t->connect_failed = 1;
        t->io_error = 1;
        return NULL;
    }

    if (t->mode & 0x4) {
        /* ---------- mode 4：红黑树节点计数测试（只跑一次） ---------- */
        int rc = rbtree_count_test(connfd, t->count);
        if (rc == -2) {
            t->io_error = 1;
        } else {
            t->failed += rc;
            t->done_rounds = t->count;
        }
    } else {
        for (int i = 0; i < t->count; i++) {
            int rc;

            if (t->mode & 0x1) {                 /* 数组引擎 */
                rc = array_testcase(connfd, t->id);
                if (rc == -2) { t->io_error = 1; break; }
                t->failed += rc;
            }

            if (t->mode & 0x2) {                 /* 红黑树引擎 */
                rc = rbtree_testcase(connfd, t->id);
                if (rc == -2) { t->io_error = 1; break; }
                t->failed += rc;
            }

            t->done_rounds++;                    /* 完整跑完一轮才计数 */
        }
    }

    close(connfd);
    return NULL;
}

/* ./testcase_bak -s 127.0.0.1 -p 9096 -m 2 -t 50 -n 100000 */
int main(int argc, char *argv[]) {

    char ip[16] = {0};
    int port = 0;
    int mode = 1;
    int threadnum = 1;
    int total_count = 100000;   /* 总轮数 */

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
                printf("Usage: %s -s <ip> -p <port> [-m 1|2|3|4] [-t threads] [-n total_rounds]\n", argv[0]);
                return -1;
        }
    }

    if (ip[0] == '\0' || port <= 0 || threadnum <= 0 || total_count <= 0 || (mode & 0x7) == 0) {
        printf("Usage: %s -s <ip> -p <port> [-m 1|2|3|4] [-t threads] [-n total_rounds]\n", argv[0]);
        printf("  -m 1 : 数组引擎   SET/GET/MOD/DEL\n");
        printf("  -m 2 : 红黑树引擎 RSET/RGET/RMOD/RDEL\n");
        printf("  -m 3 : 两种都跑\n");
        printf("  -m 4 : 红黑树节点计数测试（必须 -t 1）\n");
        return -1;
    }

    if ((mode & 0x4) && threadnum != 1) {
        printf("mode 4 必须使用 -t 1（RCOUNT 是全局计数，多线程会互相干扰）\n");
        return -1;
    }

    int per_round = ((mode & 0x1) ? 6 : 0) + ((mode & 0x2) ? 6 : 0) + ((mode & 0x4) ? 4 : 0);
    const char *mode_name = (mode & 0x4) ? "rbtree-count" :
                            (mode & 0x3) == 0x3 ? "array+rbtree" :
                            ((mode & 0x2) ? "rbtree" : "array");
    thread_arg_t *args = calloc(threadnum, sizeof(thread_arg_t));
    pthread_t *tids = calloc(threadnum, sizeof(pthread_t));
    if (!args || !tids) { perror("calloc"); return -1; }

    int base = total_count / threadnum;
    int rem  = total_count % threadnum;
    int actual_rounds = 0;
    for (int i = 0; i < threadnum; i++) {
        args[i].ip = ip;
        args[i].port = port;
        args[i].mode = mode;
        args[i].id = i;
        args[i].count = base + (i < rem ? 1 : 0);
        actual_rounds += args[i].count;
    }

    struct timeval tv_begin, tv_end;
    gettimeofday(&tv_begin, NULL);

    for (int i = 0; i < threadnum; i++) {
        pthread_create(&tids[i], NULL, test_entry, &args[i]);
        usleep(1000);
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
    long planned   = (long)actual_rounds * per_round;
    long completed = done_rounds * per_round;

    if (conn_fail > 0) {
        printf("连接失败: %d/%d 个线程连不上 %s:%d\n", conn_fail, threadnum, ip, port);
        printf("mode: %s, planned: %ld requests, completed: %ld, time_used: %d ms —— QPS 无意义，不计算\n",
               mode_name, planned, completed, time_used);
    } else if (failed > 0 || io_error) {
        printf("mode: %s, threads: %d, completed: %ld/%ld requests, failed: %d%s, time_used: %d ms —— 有失败，QPS 仅供参考\n",
               mode_name, threadnum, completed, planned, failed,
               io_error ? " (有连接中断)" : "", time_used);
    } else {
        printf("mode: %s, threads: %d, requests: %ld, failed: 0, time_used: %d ms, qps: %.0f\n",
               mode_name, threadnum, completed, time_used,
               time_used > 0 ? completed * 1000.0 / time_used : 0.0);
    }

    free(args);
    free(tids);
    return 0;
}
