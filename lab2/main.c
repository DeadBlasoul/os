#define _CRT_SECURE_NO_WARNINGS

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <inttypes.h>

#include "childprocess.h"

#define PRECALCULATED 2
#define PRECALCULATED_RANGE_ERROR UINT64_MAX
#define MAX_FIB_N 16

inline uint64_t precalculated(const size_t n) {
    switch (n) {
    case 1:
        return 0;
    case 2:
        return 1;
    }

    return PRECALCULATED_RANGE_ERROR;
}

int fib(FILE* const inp, FILE* const out, const char* const filename) {
    // Read index of number from fibonacci sequence
    size_t n;
    if (fscanf(inp, "%zu", &n) != 1) {
        return 1;
    }

    // Check that n is not too high
    if (n > MAX_FIB_N) {
        return EXIT_FAILURE;
    }

    // If n too low return precalculated
    if (n <= PRECALCULATED) {
        uint64_t result = precalculated(n);
        if (result == PRECALCULATED_RANGE_ERROR) {
            return EXIT_FAILURE;
        }

        fprintf(out, "%" PRIu64 "\n", result);
        return EXIT_SUCCESS;
    }

    // Initiate child processes
    child_process process_first;
    child_process process_second;
    create_child_process(filename, &process_first);
    create_child_process(filename, &process_second);

    // Write data to the first process
    fprintf(process_first.inp, "%zu\n", n - 1);
    fflush(process_first.inp);
    // Write data to the second process
    fprintf(process_second.inp, "%zu\n", n - 2);
    fflush(process_second.inp);

    // Read output
    uint64_t first_result;
    uint64_t second_result;
    int first_err = fscanf(process_first.out, "%" PRIu64, &first_result);
    int second_err = fscanf(process_second.out, "%" PRIu64, &second_result);

    // Check that we read successful
    if (first_err != 1 && second_err != 1) {
        return EXIT_FAILURE;
    }

    // Return result
    uint64_t result = first_result + second_result;
    fprintf(out, "%" PRIu64 "\n", result);

    return EXIT_SUCCESS;
}

int main(int argc, char* argv[]) {
    const char* const filename = argv[0];
    int err = fib(stdin, stdout, filename);
    
    return err;
}
