

#ifndef __KVSTORE_H__
#define __KVSTORE_H__


#include<stdlib.h>


#define BUFFER_LENGTH           1024

typedef int(*RCALLBACK)(int fd);



// conn , fd , buffer , callback
struct conn_item
{
    int fd;
    
    //读的buffer
    char rbuffer[BUFFER_LENGTH];
    int rlen;
    //写的buffer
    char wbuffer[BUFFER_LENGTH];
    int wlen;

    char resource[BUFFER_LENGTH]; // xxx.html
    
    //共用体 每次只能用一个，占用内存区里面最大的。节省内存
    union 
    {
        RCALLBACK accept_callback;
        RCALLBACK recv_callback;
    } recv_t;
    
    RCALLBACK send_callback;
};


int epoll_entry(void);
int ntyco_entry(void);


int kvstore_request(struct conn_item *item);

void *kvstore_malloc(size_t size);
void kvstore_free(void *ptr);

#define NETWORK_EPOLL   0
#define NETWORK_NTYCO   1
#define NETWORK_IOURING 2


#define ENABLE_ARRAY_KVENGINE   1
#define ENABLE_NETWORK_SELECT   NETWORK_EPOLL  


#if ENABLE_ARRAY_KVENGINE
struct kvs_array_item{
    char * key;
    char * value;
};
#define KVS_ARRAY_SIZE 1024
#endif

int kvstore_array_set(char *key , char *value);
char * kvstore_array_get(char * key);




#endif
