#pragma once

#include <stdint.h>

struct node_i32_t {
    int32_t value;
    struct node_i32_t* next;
    struct node_i32_t* prev;
};

struct list_i32_t {
    struct node_i32_t* head;
    struct node_i32_t* tail;
};

typedef struct node_i32_t*       list_i32_iterator_t;
typedef const struct node_i32_t* list_i32_const_iterator_t;
