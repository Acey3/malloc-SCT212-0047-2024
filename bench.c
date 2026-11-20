#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "myalloc.h"

#define OPS   50000
#define SLOTS 512
#define SEED  42

// convert two timespec structs into elapsed milliseconds
static double elapsed_ms(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000.0 +
           (end.tv_nsec - start.tv_nsec) / 1e6;
}

//runs the benchmark on my allocator with the chosen policy
static void run_custom(fit_policy_t policy, const char *label) {
    void *ptrs[SLOTS] = {0};
    struct timespec t0, t1;

    set_policy(policy);
    // reset seed so every policy sees the exact same sequence of sizes and slots
    srand(SEED);
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int i = 0; i < OPS; i++) {
        int slot = rand() % SLOTS;
        if (ptrs[slot] != NULL) {
            my_free(ptrs[slot]);
            ptrs[slot] = NULL;
        } else {
            // random size between 1 and 4096 bytes
            size_t sz = (size_t)(rand() % 4096) + 1;
            ptrs[slot] = my_malloc(sz);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);

    // snapshot heap metrics before cleaning up remaining live allocations
    heap_stats_t st;
    heap_stats(&st);
    double ext_frag = st.free_bytes > 0
        ? 1.0 - ((double)st.largest_free / (double)st.free_bytes)
        : 0.0;

    printf("| `%s` | %.2f | %zu | %zu | %zu | %zu | %.4f |\n",
           label, elapsed_ms(t0, t1), st.heap_bytes, st.free_bytes,
           st.free_blocks, st.largest_free, ext_frag);

    // free any leftover pointers so the heap stays clean
    for (int i = 0; i < SLOTS; i++) {
        if (ptrs[i] != NULL) {
            my_free(ptrs[i]);
        }
    }
}

//runs the same workload but with the normal malloc and free so we can compare the speed
static void run_libc(void) {
    void *ptrs[SLOTS] = {0};
    struct timespec t0, t1;

    // use the same seed to compare libc malloc/free on the identical workload
    srand(SEED);
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int i = 0; i < OPS; i++) {
        int slot = rand() % SLOTS;
        if (ptrs[slot] != NULL) {
            free(ptrs[slot]);
            ptrs[slot] = NULL;
        } else {
            size_t sz = (size_t)(rand() % 4096) + 1;
            ptrs[slot] = malloc(sz);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("| `libc` (`malloc`/`free`) | %.2f | — | — | — | — | — |\n",
           elapsed_ms(t0, t1));

    for (int i = 0; i < SLOTS; i++) {
        if (ptrs[i] != NULL) {
            free(ptrs[i]);
        }
    }
}

//checks which mode was typed (first, best or libc) and runs that benchmark
int main(int argc, char **argv) {
    // run each mode in its own process so sbrk heap state does not carry over
    if (argc == 2 && strcmp(argv[1], "first") == 0) {
        run_custom(FIT_FIRST, "FIT_FIRST");
    } else if (argc == 2 && strcmp(argv[1], "best") == 0) {
        run_custom(FIT_BEST, "FIT_BEST");
    } else if (argc == 2 && strcmp(argv[1], "libc") == 0) {
        run_libc();
    } else {
        fprintf(stderr, "Usage: %s [first|best|libc]\n", argv[0]);
        return 1;
    }
    return 0;
}