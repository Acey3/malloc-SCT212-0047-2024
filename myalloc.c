#define _DEFAULT_SOURCE //this is for declaring sbrk which is used to grow the heap when it runs out of space
#include<unistd.h>
#include<string.h>
#include<stdint.h>
#include<stdio.h>
#include "myalloc.h"

#define MAGIC 0x1234567 //This is a fixed number put in every header so we can tell real blocks from invalid pointers

static fit_policy_t policy = FIT_FIRST; //setting the default policy to first fit

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

void set_policy(fit_policy_t p) {// used to choose between first fit and best fit
    policy = p;
}
// it uses both first fit and best fit policies to find a free block that is large enough to fit the requested size.
static block_t *find_fit(size_t need) {
    block_t *best = NULL;
    for (block_t *curr = head; curr != NULL; curr = curr->next) {
        if (!curr->free || curr->size < need) {
            continue;
        }
        if (policy == FIT_FIRST) {
            return curr;
        }
        if (best == NULL || curr->size < best->size) {
            best = curr;
        }
    }
    return best;
}

// splitting is where it finds a free block and split it into two blocks onre for requested space and the other free and it should be able to be a head
static void split_block(block_t *b, size_t need) {
    if (b->size < need + HDR + 16) {
        return; // too small to split; give away the entire block
    }

    // place the new header right after the `need` bytes of payload
    block_t *rem = (block_t *)((char *)payload(b) + need);
    rem->size = b->size - need - HDR;
    rem->next = b->next;
    rem->free = 1;
    rem->magic = MAGIC;

    b->size = need;
    b->next = rem;

    if (b == tail) {
        tail = rem;
    }
}

void *my_malloc(size_t size) { //used to allocate memory of the requested size.
    if (size == 0) {
        return NULL;
    }

    size_t need = align_up(size);
    if (need < size) {  // overflow check when the need is less than size it returns NULL
        return NULL;
    }

    //scans the list for the first free block tthat is of the reuired size if there isn't it grows the heap(asks for more memory from the os)
    block_t *b = find_fit(need);
    if (b == NULL) {
        if (grow(need) == NULL) {
            return NULL;
        }
        b = find_fit(need);
        if (b == NULL) {
            return NULL;
        }
    }

    split_block(b, need);
    b->free = 0;
    return payload(b);
}

// coalesce is used to merge free blocks that are adjacent to each other
static void coalesce(void) {
    block_t *curr = head;
    while (curr != NULL && curr->next != NULL) {
        block_t *nxt = curr->next;
        // only merge if both are free and nxt starts right where the current block's payload ends
        if (curr->free && nxt->free &&
            (char *)payload(curr) + curr->size == (char *)nxt) {
            curr->size += HDR + nxt->size;
            curr->next = nxt->next;
            if (nxt == tail) {
                tail = curr;
            }
            // stay on curr in case the new neighbor is also free for instance if we have three free blocks in a row
        } else {
            curr = curr->next;
        }
    }
}

void my_free(void *ptr) { //this is used to validate the users pointer and mark the block as free and coalesce adjacent free blocks
    if (ptr == NULL) {
        return;
    }

    block_t *b = header(ptr);

    if (b->magic != MAGIC) { //wrong magic number means that the pointer is inavalid since the it should match with the magic number in header.
        const char msg[] = "my_free: invalid pointer\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        return;
    }

    if (b->free == 1) {
        const char msg[] = "my_free: double free detected\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1); // this is used to send error messages straight to the file descriptor STDERR .
        return;
    }

    b->free = 1; //the free is for freeing the block and marks it free and available so that it can be reusablle by future malloc() calls
    coalesce();
}

void *my_calloc(size_t n, size_t size) {//contigous allocation: it is used to allocate memory for n items and set it all to zero
    
    if (n != 0 && size > SIZE_MAX / n) {
        return NULL;
    }

    size_t total = n * size;
    void *ptr = my_malloc(total);  //alloccates memory.
    if (ptr == NULL) {
        return NULL;
    }

    memset(ptr, 0, total);
    return ptr;
}

void *my_realloc(void *ptr, size_t size) { //reallocation....this resizes an existing allocation to a pointer to the required size.
    // case 1: NULL pointer behaves like my_malloc(size)
    if (ptr == NULL) {
        return my_malloc(size);
    }

    // case 2: size 0 frees the block and returns NULL
    if (size == 0) {
        my_free(ptr);
        return NULL;
    }

    block_t *b = header(ptr);
    if (b->magic != MAGIC || b->free == 1) { //rejects invalid pointers
        return NULL;
    }

    size_t need = align_up(size);
    if (need < size) {
        return NULL;
    }

    // case 3: current block is already big enough; split unused tail if possible
    if (b->size >= need) {
        split_block(b, need);
        coalesce(); // coalesce in case the split created a new free block that can be merged with its neighbor
        return ptr;
    }

    // case 4: next block is free, physically adjacent, and makes up the difference
    block_t *nxt = b->next;
    if (nxt != NULL && nxt->free &&
        (char *)payload(b) + b->size == (char *)nxt &&
        b->size + HDR + nxt->size >= need) {
        b->size += HDR + nxt->size;
        b->next = nxt->next;
        if (nxt == tail) {
            tail = b;
        }
        split_block(b, need);
        return ptr;
    }

    // case 5: allocate a new block, copy existing data, and free the old block
    void *new_ptr = my_malloc(size);
    if (new_ptr == NULL) {
        return NULL;
    }

    size_t copy_bytes = b->size < size ? b->size : size;
    memcpy(new_ptr, ptr, copy_bytes);
    my_free(ptr);
    return new_ptr;
}
void heap_stats(heap_stats_t *out) {// this is used to fill the heap_stats_t structure with the current state of the heap and it is used for testing and debugging purposes
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->heap_bytes = heap_bytes;

    for (block_t *curr = head; curr != NULL; curr = curr->next) {
        out->blocks++;
        if (curr->free) {
            out->free_blocks++;
            out->free_bytes += curr->size;
            if (curr->size > out->largest_free) {
                out->largest_free = curr->size;
            }
        } else {
            out->used_bytes += curr->size;
        }
    }
}
void heap_dump(void) {// this is used to print every block in the heap using write(2) and it is used for debugging and testing purposes
    char buf[128];
    int idx = 0;
    for (block_t *curr = head; curr != NULL; curr = curr->next, idx++) {
        int n = snprintf(buf, sizeof(buf), "[%2d] %p size=%zu %s\n",
                         idx, (void *)curr, curr->size,
                         curr->free ? "FREE" : "USED");
        if (n > 0) {
            size_t len = (size_t)n < sizeof(buf) ? (size_t)n : sizeof(buf) - 1;
            write(STDOUT_FILENO, buf, len);
        }
    }
}

int heap_check(void) { // checks that the heap is consistent, returns 1 if ok and 0 if something is wrong
    for (block_t *curr = head; curr != NULL; curr = curr->next) {
        if (curr->magic != MAGIC || (curr->free != 0 && curr->free != 1)) {
            return 0; // invalid header
        }
        if (((uintptr_t)payload(curr) % 16) != 0 || (curr->size % 16) != 0) {
            return 0; // misaligned payload
        }
        if (curr->next != NULL) {
            if ((uintptr_t)curr->next < (uintptr_t)payload(curr) + curr->size) {
                return 0; // not in increasing address order
            }
            if (curr->free && curr->next->free &&
                (char *)payload(curr) + curr->size == (char *)curr->next) {
                return 0; // adjacent free blocks left unmerged
            }
        }
    }
    return 1;
}
