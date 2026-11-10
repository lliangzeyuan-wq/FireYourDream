

#include"kvstore.h"
#include<string.h>
struct kvs_array_item array_table[KVS_ARRAY_SIZE] = {0};
int array_idx = 0;
int kvstore_array_set(char *key , char *value)
{
    if(key == NULL || value == NULL || array_idx == KVS_ARRAY_SIZE) return -1;

    char *kcopy = kvstore_malloc(strlen(key)+1);
    if(kcopy == NULL) return -1;
    strncpy(kcopy , key , strlen(key)+1);

    char *vcopy = kvstore_malloc(strlen(value) +1);
    if(vcopy == NULL)
    {
        kvstore_free(kcopy);
        return -1;
    }
    strncpy(vcopy , value , strlen(value)+1);

    array_table[array_idx].key = kcopy;
    array_table[array_idx].value = vcopy;
    array_idx++;
    return 0;
}

char * kvstore_array_get(char * key)
{
    int i = 0 ;

    for(i=0 ; i < array_idx ; ++i)
    {
        if(strcmp(array_table[i].key , key) == 0)
        {
            return array_table[i].value;
        }
    }
    return NULL;
}    

