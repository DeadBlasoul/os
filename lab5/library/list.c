#include "list_structs.h"

#include <string.h>
#include <stdlib.h>

#ifdef DLL_EXPORTS
    #define EXPORT __declspec(dllexport)
#else
    #define EXPORT extern
#endif

EXPORT void init_list(struct list_i32_t* list) {
    memset(list, 0x00, sizeof(struct list_i32_t));
}

EXPORT void dest_list(struct list_i32_t* list) {
    struct node_i32_t* curr = NULL;
    struct node_i32_t* next = list->head;

    while(next) {
        curr = next;
        next = curr->next;
        free(curr);
    }

    memset(list, 0x00, sizeof(struct list_i32_t));
}