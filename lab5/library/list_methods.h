#pragma once

#include <stdint.h>

#ifdef DLL_EXPORTS
    #define IMPORT __declspec(dllimport)
#else
    #define IMPORT extern
#endif

struct node_i32_t;
struct list_i32_t;

typedef struct node_i32_t*       list_i32_iterator_t;
typedef const struct node_i32_t* list_i32_const_iterator_t;

IMPORT void init_list(struct list_i32_t* list);
IMPORT void dest_list(struct list_i32_t* list);

IMPORT list_i32_iterator_t gbeg_list(struct list_i32_t* list);
IMPORT list_i32_iterator_t gend_list(struct list_i32_t* list);
IMPORT list_i32_iterator_t next_list(struct list_i32_t* list);

IMPORT int32_t* sval_list(list_i32_iterator_t it, const int32_t* value);
IMPORT int32_t* gval_list(list_i32_iterator_t it);

IMPORT void insr_list(struct list_i32_t* list, list_i32_iterator_t it, const int32_t* value);
IMPORT void eras_list(struct list_i32_t* list, list_i32_iterator_t it);