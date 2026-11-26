

#include"kvstore.h"
#include<string.h>
/* 动态分配的数组表：由 create 分配、destory 释放 */
static struct kvs_array_item *array_table = NULL;
static int array_idx = 0;

/* create：分配数组引擎 */
int kvstore_array_create(void) {

    if (array_table != NULL) return 0;      /* 已经创建过，避免重复分配 */

    array_table = calloc(KVS_ARRAY_SIZE, sizeof(struct kvs_array_item));
    if (array_table == NULL) return -1;

    array_idx = 0;
    return 0;
}

/* destory：释放所有 key/value 以及数组本身 */
void kvstore_array_destory(void) {

    int i = 0;

    if (array_table == NULL) return;

    for (i = 0; i < array_idx; i ++) {
        if (array_table[i].key != NULL) {
            kvstore_free(array_table[i].key);
            array_table[i].key = NULL;
        }
        if (array_table[i].value != NULL) {
            kvstore_free(array_table[i].value);
            array_table[i].value = NULL;
        }
    }

    kvstore_free(array_table);
    array_table = NULL;
    array_idx = 0;
}




int kvstore_array_set(char *key , char *value)
{
    int i = 0;

    /* =================【改动 1】=================
     * 先只做 NULL 检查。
     * 注意：容量检查（array_idx == KVS_ARRAY_SIZE）挪到后面去了，
     *       因为"更新已存在的 key"不需要新槽位，数组满了也应该能更新。
     * ============================================ */
    if (array_table == NULL || key == NULL || value == NULL) return -1;

    /* =================【改动 2】=================
     * 新增：先按 key 查一遍。
     * 已存在 → 只替换 value，直接 return，不追加重复条目、不增加 array_idx。
     * ============================================ */
    for (i = 0; i < array_idx; i ++) {
        if (array_table[i].key != NULL &&
            strcmp(array_table[i].key, key) == 0) {

            char *vcopy = kvstore_malloc(strlen(value) + 1);
            if (vcopy == NULL) return -1;
            strncpy(vcopy , value , strlen(value) + 1);

            kvstore_free(array_table[i].value);   /* 释放旧值，防止内存泄漏 */
            array_table[i].value = vcopy;
            return 0;                             /* 关键：array_idx 不变 */
        }
    }

    /* =================【改动 3】=================
     * 只有"新增"才需要检查容量。
     * ============================================ */
    if (array_idx == KVS_ARRAY_SIZE) return -1;

    /* ================= 以下基本是原来的"新增"逻辑 ================= */

    char *kcopy = kvstore_malloc(strlen(key) + 1);
    if (kcopy == NULL) return -1;
    strncpy(kcopy , key , strlen(key) + 1);

    char *vcopy = kvstore_malloc(strlen(value) + 1);
    if (vcopy == NULL)
    {
        kvstore_free(kcopy);
        return -1;
    }
    strncpy(vcopy , value , strlen(value) + 1);

    // 如果位置为 NULL，插入（复用 DEL 留下的空槽）
    for (i = 0; i < array_idx; i ++) {
        if (array_table[i].key == NULL) {
            array_table[i].key = kcopy;
            array_table[i].value = vcopy;
            return 0;        /* 【保持不变】这里不要 array_idx++，空槽已经算在 array_idx 里了 */
        }
    }

    /* =================【改动 4】=================
     * 原来的 if (i < KVS_ARRAY_SIZE && i == array_idx) 是冗余的：
     *   - for 循环结束后 i 一定等于 array_idx
     *   - 容量在函数开头已经检查过
     * 所以直接追加即可。
     * ============================================ */
    LOG("array idx: %d\n", array_idx);

    array_table[array_idx].key = kcopy;
    array_table[array_idx].value = vcopy;
    array_idx ++;

    return 0;
}



char * kvstore_array_get(char * key)
{
    int i = 0 ;
    if(array_table == NULL || key == NULL) return NULL;
    
    for(i=0 ; i < array_idx ; ++i)
    {
        if(array_table[i].key == NULL){

            continue;
        }    
        if(strcmp(array_table[i].key , key) == 0)
        {
            return array_table[i].value;
        }
    }
    return NULL;
}    

//if return > 0 : no exist
int kvstore_array_delete(char * key)
{
    int i = 0 ;

    if(array_table == NULL || key == NULL) return -1;

    for(i=0 ; i < array_idx ; ++i)
    {
        if(array_table[i].key == NULL) continue;
        if(strcmp(array_table[i].key , key) == 0)
        {
            kvstore_free(array_table[i].value);
            array_table[i].value = NULL;

            kvstore_free(array_table[i].key);
            array_table[i].key = NULL;


            return 0;
        }
    }
    return i;
}    

int kvstore_array_modify(char *key, char *value){
    int i=0;
    if(array_table == NULL || key == NULL || value == NULL) return -1;

    for(i=0; i<array_idx; ++i){
        if(array_table[i].key == NULL) continue;
        if(strcmp(array_table[i].key, key) == 0){
            char *vcopy = kvstore_malloc(strlen(value) + 1);
            if(vcopy == NULL) return -1;
            strncpy(vcopy, value, strlen(value)+1);

        
            kvstore_free(array_table[i].value);
            array_table[i].value = vcopy;
            return 0;
        }
    }    

    return i;
}



int kvstore_array_count(void) {

    int i, n = 0;

    if (array_table == NULL) return 0;

    for (i = 0; i < array_idx; i ++) {
        if (array_table[i].key != NULL) {
            n ++;
        }
    }

    return n;
}


