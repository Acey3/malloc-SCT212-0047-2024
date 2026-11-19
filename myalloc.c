#define _DEFAULT_SOURCE //this is for declaring sbrk which is used to grow the heap when it runs out of space
#include<unistd.h>
#include<string.h>
#include<stdint.h>
#include "myalloc.h"

#define MAGIC 0xC0FFEE

typedef struct block {
    size_t size;
    struct block *next; //stores the size of the usable payload memory
    int free; //it is used to check if the block is reusable if it is free(1) it can be re used by a future malloc()
    int magic; //this is like a debugger if it doesn't match the header it may be abuffer overflow or someone passed an invalid pointer to free
} block_t;

//it said that  every pointer returned should be divisble by 16 since it is a 64 bit cpu thus it will be fast 
static inline size_t align_up(size_t n) {
    return (n + 15) & ~(size_t)15;
}

/* header size rounded up to 16, so every payload stays 16-byte aligned */
#define HDR align_up(sizeof(block_t))

/* header address -> the pointer we give to the user */
static inline void *payload(block_t *b) {
    return (char *)b + HDR;
}

/* user pointer -> back to its header */
static inline block_t *header(void *p) {
    return (block_t *)((char *)p - HDR);
}


/*#ifdef TEST_TASK1
#include <assert.h>
int main(void) {
    assert(align_up(1) == 16);
    assert(align_up(16) == 16);
    assert(align_up(17) == 32);
    assert(HDR % 16 == 0);
    char buf[] = "task 1 ok\n";
    write(1, buf, sizeof buf - 1);
    return 0;
}
#endif*/