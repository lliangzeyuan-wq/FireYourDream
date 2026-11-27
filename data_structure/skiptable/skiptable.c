#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>

#define MAX_LEVEL 6

typedef struct Node {
    int key;
    struct Node** forward; //指针数组：forward[i] 代表该节点在第i层指向的下一个节点
} Node;

typedef struct SkipList {
    int level; // 跳表当前实际最高层数（注意：小于等于MAX_LEVEL）
    Node* header; // 哑头结点，是每一层链表的起点，不存储有效数据
} SkipList;



Node* createNode(int level, int key) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->key = key;
    newNode->forward = (Node**)malloc(sizeof(Node*) * (level + 1));
    for (int i = 0; i <= level; ++i) {
        newNode->forward[i] = NULL;
    }
    return newNode;
}

SkipList* createSkipList() {
    SkipList* skipList = (SkipList*)malloc(sizeof(SkipList));
    skipList->level = 0;
    skipList->header = createNode(MAX_LEVEL, INT_MIN);
    return skipList;
}

int randomLevel() {
    int level = 0;
    while (rand() < RAND_MAX / 2 && level < MAX_LEVEL) {
        level++;
    }
    return level;
}

void insert(SkipList* skipList, int key) {
    Node* update[MAX_LEVEL + 1];
    Node* current = skipList->header;

    for (int i = skipList->level; i >= 0; --i) {
        while (current->forward[i] != NULL && current->forward[i]->key < key) {
            current = current->forward[i];
        }
        update[i] = current;
    }
    current = current->forward[0];
    if (current == NULL || current->key != key) {
        int level = randomLevel();
        if (level > skipList->level) {
            for (int i = skipList->leve,l + 1; i <= level; ++i) {
                update[i] = skipList->header;
            }
            skipList->level = level;
        }

        Node* newNode = createNode(level, key);
        for (int i = 0; i <= level; ++i) {
            newNode->forward[i] = update[i]->forward[i];
            update[i]->forward[i] = newNode;
        }
        printf("Successfully inserted key %d\n", key);
    }
}

bool search(SkipList* skipList, int key) {
    Node* current = skipList->header;

    for (int i = skipList->level; i >= 0; --i) {
        while (current->forward[i] != NULL && current->forward[i]->key < key) {
            current = current->forward[i];
        }
    }
    current = current->forward[0];
    if (current != NULL && current->key == key) {
        printf("Key %d found\n", key);
        return true;
    }
    printf("Key %d not found\n", key);
    return false;
}

void deleteNode(SkipList* skipList, int key) {
    Node* update[MAX_LEVEL + 1];
    Node* current = skipList->header;

    for (int i = skipList->level; i >= 0; --i) {
        while (current->forward[i] != NULL && current->forward[i]->key < key) {
            current = current->forward[i];
        }
        update[i] = current;
    }
    current = current->forward[0];
    if (current != NULL && current->key == key) {
        for (int i = 0; i <= skipList->level; ++i) {
            if (update[i]->forward[i] != current)
                break;
            update[i]->forward[i] = current->forward[i];
        }
        free(current);

        while (skipList->level > 0 &&
               skipList->header->forward[skipList->level] == NULL) {
            skipList->level--;
        }
        printf("Successfully deleted key %d\n", key);
    }
}

void printSkipList(SkipList* skipList) {
    printf("Skip List:\n");
    for (int i = 0; i <= skipList->level; ++i) {
        Node* node = skipList->header->forward[i];
        printf("Level %d: ", i);
        while (node != NULL) {
            printf("%d ", node->key);
            node = node->forward[i];
        }
        printf("\n");
    }
}



int main() {
    SkipList* skipList = createSkipList();

    insert(skipList, 3);
    insert(skipList, 6);
    insert(skipList, 2);
    insert(skipList, 9);

    printSkipList(skipList);
    search(skipList, 6);
    deleteNode(skipList, 2);
    printSkipList(skipList);

    return 0;
}
