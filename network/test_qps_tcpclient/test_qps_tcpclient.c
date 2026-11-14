
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>

#include <arpa/inet.h>


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

#define TEST_MESSAGE        "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuvwxyz\r\n"
#define RBUFFER_LENGTH      128

int send_recv_tcppkt(int fd){

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
    return 0;
}

// ./test_qps_tcpclient -s 127.0.0.1 -p 2048 -t 50 -c 100 -n 10000
int main(int argc, char *argv[]){

    char serverip[16] = {0};
    int port;
    int threadnum = 0;
    int connection = 0;
    int requestion = 0;
    
    int opt;

    while((opt = getopt(argc, argv, "s:p:t:c:n:")) != -1){

        switch (opt){
            case 's':
                printf("-s: %s\n", optarg);
                strcpy(serverip, optarg);
                break;
            case 'p':
                printf("-p: %s\n", optarg);
                port = atoi(optarg);
                break;
            case 't':
                printf("-t: %s\n", optarg);
                threadnum = atoi(optarg);
                break;
            case 'c':
                printf("-c: %s\n", optarg);
                connection = atoi(optarg);
                break;
            case 'n':
                printf("-n: %s\n", optarg);
                requestion = atoi(optarg);
                break;
            default:
                return -1;
        }
    }


    int connfd = connect_tcpserver(serverip, port);
    if (connfd < 0) {
        printf("connect_tcpserver failed\n");
        return -1;
    }
    
    int ret = send_recv_tcppkt(connfd);
    if (ret != 0) {
        printf("send_recv_tcppkt failed\n");
        return -1;
    }

    printf("success\n");

}
