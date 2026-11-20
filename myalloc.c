#define _DEFAULT_SOURCE //this is for declaring sbrk which is used to grow the heap when it runs out of space
#include<unistd.h>
#include<string.h>
#include<stdint.h>
#include "myalloc.h"

#define MAGIC 0x1234567 //This is a fixed number put in every header so we can tell real blocks from invalid pointers

typedef struct block {
    size_t size; //stores the size of the usable payload memory
    struct block *next; 
    int free; //it is used to check if the block is reusable if it is free(1) it can be re used by a future malloc()
    int magic; 
} block_t;

//the n should be rounded up to the nearest multiple of 16 so that the payload is aligned to 16 bytes
static inline size_t align_up(size_t n) {
    return (n + 15) & ~(size_t)15;
}

//since this is 24 bytes it is rounded up to 32 bytes so the payload will be aligned to 16 bytes
#define HDR align_up(sizeof(block_t))

//this returns the payload pointer from the header pointer
static inline void *payload(block_t *b) {
    return (char *)b + HDR;
}

//this returns the header pointer from the payload pointer
static inline block_t *header(void *p) {
    return (block_t *)((char *)p - HDR);
}

#define MIN_CHUNK (64 * 1024) //used to request at least 64 KB so as to prevent sbrk() for being called for small allocations

static block_t *head = NULL;
static block_t *tail = NULL;
static size_t heap_bytes = 0;

static block_t *grow(size_t need) {
    //this makes sure the payload size is a multiple of 16 and the total chunk is at least 64 KB
    size_t payload_size = align_up(need);
    if (HDR + payload_size < MIN_CHUNK) {
        payload_size = MIN_CHUNK - HDR;
    }

    // This is used to check the current heap break and calculate the padding needed to align the payload to 16 bytes
    void *cur = sbrk(0);
    if (cur == (void *)-1) {
        return NULL;
    }

    size_t pad = align_up((uintptr_t)cur) - (uintptr_t)cur;
    size_t total = pad + HDR + payload_size;

    //sbrk() is used to request the new chunk of memory from the OS when the heap runs out of space. inshort it is used to grow the heap.
    void *raw = sbrk((intptr_t)total);
    if (raw == (void *)-1) {
        return NULL;
    }

    heap_bytes += total;

    // This is used to create a new block header at the aligned address and initialize its fields.
    block_t *b = (block_t *)((char *)raw + pad);
    b->size = payload_size;
    b->next = NULL;
    b->free = 1;
    b->magic = MAGIC;

    // insertion occurs where a new block is added to the end of the free list 
    if (head == NULL) {
        head = b;
        tail = b;
    } else {
        tail->next = b;
        tail = b;
    }

    return b;
}



 