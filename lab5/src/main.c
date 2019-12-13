#include <list.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define CAST_STR(x) #x
#define STR(x) CAST_STR(x)

static void skipline();

static void print(void*);
static void insert(void*);
static void erase(void*);
static void clear(void*);

#define CMD_NAME_SIZE 16

typedef void (*callback_t)(void*);

struct command_t {
    char       name[CMD_NAME_SIZE];
    callback_t callback;
};

#define CMD_END_ELEM { { '\0' }, NULL }

bool test_cmd(const struct command_t* command) {
    return command->callback != NULL;
}

struct interpreter_t {
    void*            userdata;
    struct command_t commands[];
}
g_interpreter = {
    .userdata = NULL,
    .commands = {
        {.name = "print",  .callback = print},
        {.name = "insert", .callback = insert},
        {.name = "erase",  .callback = erase},
        {.name = "clear",  .callback = clear},
        CMD_END_ELEM
    }
};

static int run();

int main() {
    struct list_i32_t list;
    init_list(&list);

    g_interpreter.userdata = &list;
    const int err = run();

    dest_list(&list);
    return err;
}

void skipline() {
    int skip;
    while (skip = getchar(), skip != '\n' && skip != EOF)
    {}
}

void print(void* data) {
    printf("print called\n");
}

void insert(void* data) {
    printf("insert called\n");
}

void erase(void* data) {
    printf("erase called\n");
}

void clear(void* data) {
    printf("clear called\n");
}

int run() {
    char cmd_str[CMD_NAME_SIZE] = { '\0' };
    bool done                   = false;

    while(!done) {
        if (feof(stdin)) {
            done = true;
            continue;
        }

        if (scanf_s("%" STR(CMD_NAME_SIZE) "s", cmd_str) != 1) {
            printf("incorrect input\n");
            skipline();
            continue;
        }

        if (!strncmp(cmd_str, "exit", CMD_NAME_SIZE)) {
            done = true;
            continue;
        }

        bool found = false;
        for (size_t i = 0; test_cmd(&g_interpreter.commands[i]); ++i) {
            const struct command_t* command = &g_interpreter.commands[i];
            if (!strncmp(cmd_str, command->name, CMD_NAME_SIZE)) {
                found = true;
                command->callback(g_interpreter.userdata);
                break;
            }
        }
        if (!found) {
            printf("command '%s' not found\n", cmd_str);
        }
        skipline();
    }

    return EXIT_SUCCESS;
}
