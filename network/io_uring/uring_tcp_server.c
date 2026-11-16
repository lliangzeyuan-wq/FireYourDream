
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <liburing.h>
#include <string.h>
#include <unistd.h>

#define ENTRIES_LENGTH      128

enum {
    READ,
    WRITE,
    ACCEPT,
};


struct conninfo {
    int connfd;
    int type;
};    


void set_read_event(struct io_uring *ring, int fd, void *buf, size_t len, int flags){
    //函数返回值是sq中可以存储数据的首地址
        struct io_uring_sqe *sqe = io_uring_get_sqe(ring);

        if(sqe == NULL){
            printf("SQ 队列已满，获取 sqe 失败\n");
            return; 
        }
    
        io_uring_prep_recv(sqe, fd, buf, len, flags);

        //ssize_t recv(int sockfd, void *buf, size_t len, int flags);
    
        struct conninfo ci = {
            .connfd = fd,
            .type = READ
        };    
    
        //
        memcpy(&sqe->user_data, &ci, sizeof(struct conninfo));
}



void set_write_event(struct io_uring *ring, int fd, const void *buf, size_t len, int flags){
    //函数返回值是sq中可以存储数据的首地址
        struct io_uring_sqe *sqe = io_uring_get_sqe(ring);

        if(sqe == NULL){
            printf("SQ 队列已满，获取 sqe 失败\n");
            return; 
        }
    
        io_uring_prep_send(sqe, fd, buf, len, flags);

        //ssize_t recv(int sockfd, void *buf, size_t len, int flags);
    
        struct conninfo ci = {
            .connfd = fd,
            .type = WRITE
        };    
    
        //
        memcpy(&sqe->user_data, &ci, sizeof(struct conninfo));
}


void set_accept_event(struct io_uring *ring, int fd, 
struct sockaddr *cliaddr, socklen_t *clilen, unsigned flags){
    //函数返回值是sq中可以存储数据的首地址
        struct io_uring_sqe *sqe = io_uring_get_sqe(ring);

        if(sqe == NULL){
            printf("SQ 队列已满，获取 sqe 失败\n");
            return; 
        }
    
        io_uring_prep_accept(sqe, fd, cliaddr,
           clilen, flags);
    
        //int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
    
        struct conninfo ci = {
            .connfd = fd,
            .type = ACCEPT
        };    
    
        //
        memcpy(&sqe->user_data, &ci, sizeof(struct conninfo));
}

int main(){


    int listenfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (listenfd == -1) return -1;
    // listenfd
    struct sockaddr_in servaddr, clientaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(9999);

    if (-1 == bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr))) {
        return -2;
    }


    listen(listenfd, 1024);

    //定义一个参数结构体
    struct io_uring_params params;
    memset(&params, 0, sizeof(params));
    

    struct io_uring ring;
    //初始化sq和cq两个队列  ,ring的作用就是当sq已经弄完了之后，把
    //   sq中的元素移动到cq中

    io_uring_queue_init_params(ENTRIES_LENGTH, &ring, &params);


    socklen_t clilen = sizeof(clientaddr);
    set_accept_event(&ring, listenfd, (struct sockaddr*)&clientaddr, &clilen, 0);

    
    char buffer[1024] = {0};

    while(1){
        struct io_uring_cqe *cqe;
        //将当前已写入提交队列（SQ）但尚未提交的 I/O 请求全部发送给内核去执行。
        io_uring_submit(&ring);
        //阻塞等待，一旦内核完成至少一个任务并将结果放入 CQ，该函数返回
        int ret = io_uring_wait_cqe(&ring, &cqe);

        struct io_uring_cqe *cqes[10];
        //批量读取， 最多读取10个，如果没有足够的时间，不会阻塞（非阻塞）
        int cqecount = io_uring_peek_batch_cqe(&ring, cqes, 10);

        int i = 0;
        unsigned count = 0;
        for(i = 0; i < cqecount; ++i){
            cqe = cqes[i];
            count++;
            struct conninfo ci;
            memcpy(&ci, &cqe->user_data, sizeof(ci));

            if(ci.type == ACCEPT){
                int connfd = cqe->res;
                if (connfd < 0) {
                    printf("accept failed: %d\n", connfd);
                } else {
                    set_read_event(&ring, connfd, buffer, 1024, 0);
                }
                // 关键：每接受一个连接，必须再挂一个 accept，否则只能接第一个
                set_accept_event(&ring, listenfd, (struct sockaddr*)&clientaddr, &clilen, 0);
            }else if(ci.type == READ){
                
                //这里是READ， 因此cqe->res就是读到的字节数
                int bytes_read = cqe->res;
                if(bytes_read == 0){
                    close(ci.connfd);
                }else if(bytes_read < 0){

                }else{
                    printf("buffer : %s\n", buffer);
                    set_write_event(&ring, ci.connfd, buffer, bytes_read, 0);
                }    
               
            }else if (ci.type == WRITE){
                set_read_event(&ring, ci.connfd, buffer, 1024, 0);
            }    
            //告知内核结果已经处理完了，可以对坑位进行处理了，一次处理一格
           // io_uring_cqe_seen(&ring, cqe);

        }    
        //一次性告诉内核，已经把count个结果全部处理完了，可以把坑位收回了
        io_uring_cq_advance(&ring, count);
        
    }    


    
    getchar();


}
