#ifndef __KVSTORE_H__
#define __KVSTORE_H__



#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/socket.h>
#include<stdlib.h>


#define BUFFER_LENGTH           512

#define ENABLE_LOG      0
#if ENABLE_LOG
#define LOG(_fmt, ...) fprintf(stdout, "[%s:%d]: "_fmt, __FILE__, __LINE__,##__VA_ARGS__)
#else
#define LOG(_fmt, ...) do{}while(0)
#endif


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
#define ENABLE_NETWORK_SELECT   NETWORK_NTYCO


#define ENABLE_ARRAY_KVENGINE   1
#define ENABLE_RBTREE_KVENGINE   1




#if ENABLE_ARRAY_KVENGINE
struct kvs_array_item{
    char * key;
    char * value;
};
#define KVS_ARRAY_SIZE 1024
#endif

int  kvstore_array_create(void);
void kvstore_array_destory(void);
int kvstore_array_set(char *key , char *value);
char * kvstore_array_get(char * key);
int kvstore_array_delete(char * key);
int kvstore_array_modify(char *key, char *value);
int kvstore_array_count(void);



#if ENABLE_RBTREE_KVENGINE

typedef struct _rbtree rbtree_t;


//extern ： 声明，不分配内存
extern rbtree_t Tree;

int kvstore_rbtree_create(rbtree_t *tree);
void kvstore_rbtree_destory(rbtree_t *tree);
int kvs_rbtree_set(rbtree_t *tree, char *key, char *value);
char* kvs_rbtree_get(rbtree_t *tree, char *key);
int kvs_rbtree_delete(rbtree_t *tree, char *key);
int kvs_rbtree_modify(rbtree_t *tree, char *key, char *value);
int kvs_rbtree_count(rbtree_t *tree);

#endif


#endif
