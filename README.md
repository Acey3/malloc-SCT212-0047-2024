# Memory Allocator

**Name:** Patrick Kamau
**Reg No:** SCT212-0047/2024

## How to Build and Run

Run the test harness:
```
make
```

Run the benchmark (Task 8):
```
make bench
```
or by hand:
```
gcc -std=c11 -Wall -Wextra -g -O0 -o bench bench.c myalloc.c
./bench first
./bench best
./bench libc
```

Run the tests with AddressSanitizer and UndefinedBehaviorSanitizer:
```
gcc -std=c11 -g -fsanitize=address,undefined -o t_asan myalloc.c test_myalloc.c
./t_asan
```

Remove build output:
```
make clean
```

## Features Implemented

| Task | Feature | Status |
|------|---------|--------|
| 1 | Block header, `align_up`, `payload`, `header`, `HDR` | Complete |
| 2 | Growing the heap with `sbrk` (`grow`) | Complete |
| 3 | `my_malloc` with first-fit | Complete |
| 4 | Block splitting | Complete |
| 5 | `my_free` with coalescing, invalid pointer and double free detection | Complete |
| 6 | `my_calloc` (with overflow check) and `my_realloc` | Complete |
| 7 | Best-fit, `heap_stats`, `heap_dump`, `heap_check` | Complete |
| 8 | Benchmark (`bench.c`) and report (`REPORT.md`) | Complete |

## Test Results

Last lines of `make`:
```
T8 randomized stress test
  PASS  no data corruption during 20000 operations
  PASS  everything freed
  PASS  fully coalesced (few free blocks remain)
  PASS  heap consistent

Result: 29 passed, 0 failed
```
All 29 checks also pass under AddressSanitizer and UndefinedBehaviorSanitizer.

## Design Explanation

Every block in my allocator starts with a block_t header containing four fields: size stores the usable payload bytes, next points to the next block in memory, free marks whether the block is available for use so that it can be used by a future malloc(), and magic holds a constant (0x1234567) to detect invalid pointers also a way of preventing buffer overflow . On 64-bit Linux, the struct is 24 bytes, so I round it up to HDR (32 bytes) to keep a 16-byte boundary so that the cpu doesnt become slow since 64 bit cpus requires pointer to sit a memory addresses divisble by 16. When my_malloc returns a pointer, it points to the payload right after HDR. my_free steps back HDR bytes to reach the header. All blocks, both used and free, are kept in one linked list ordered by increasing memory address. Global head and tail pointers track the start and end of this list. When find_fit finds no suitable block, grow() calls sbrk() for at least 64 KB and appends a new free block to tail.Sbrk() is used to grow the heap when it runs out of space. With first-fit, find_fit returns the first free block that fits; with best-fit, it scans the whole list for the smallest fitting block. If the chosen block has at least HDR + 16 spare bytes, split_block carves the remainder into a new free block.After every my_free, coalesce walks the list from head. It merges curr and curr->next only if both are free and (char *)payload(curr) + curr->size == (char *)curr->next. Being next in the list is not enough because libc's malloc can leave a gap between our sbrk chunks. When merging, I add HDR + curr->next->size to curr->size because the second header becomes free payload too. I stay on curr after a merge in case the new neighbor is also free, for instance if we have three free blocks in a row.

## Questions

**1. Why must the header size be a multiple of 16 as well as the payload sizes?**
This is because the cpu that is runs these processes are mostly 64 bit cpu's which requires the pointers or data types to be a multiple of 16 since they sit at memory addresses divisible by 16 otherwise the cpus slow down.

**2. Why can printf() inside my_malloc() cause infinite recursion?**
On its first call, printf() allocates an internal output buffer on the heap by calling malloc(). If my_malloc() (or a malloc wrapper) calls printf(), printf() calls right back into my_malloc(), which calls printf() again until the stack overflows. Formatting into a local char buffer with snprintf() and printing with write() avoids stdio heap allocation completely.

**3. Your my_free() is O(n). Which extra field would let you find a block's previous neighbor in O(1), and what is the name of the technique?**
Adding a struct block *prev pointer to block_t turns the list into a doubly-linked list, letting my_free() jump directly to b->prev in $O(1)$ without walking from head. Alternatively, storing a boundary tag (a footer at the end of each block duplicating its size and free flag) lets my_free() look just before b's header to find the previous block in $O(1)$.

**4. Explain the difference between internal and external fragmentation, and name one place in your allocator where each one comes from.**
Internal fragmentation is wasted space inside an allocated block that the user never asked for,for instance, in my allocator, align_up() rounds a 17-byte request up to 32 bytes, and split_block() leaves remainders smaller than HDR + 16 bytes attached to the used block. External fragmentation is free memory split into scattered holes between live blocks; for instance, when alternating blocks are freed and the remaining used blocks stop coalesce() from merging those holes together.

**5. Why does sbrk() memory never get returned to the OS in your allocator? Describe how you could give back memory at the end of the heap.**
My grow() function only calls sbrk(+inc) to push the program break upward, and my_free() never calls sbrk(-dec) to lower it. To return trailing free memory, my_free() could check if tail->free is 1, verify (char *)payload(tail) + tail->size == sbrk(0) so nothing else moved the break after it, and if tail->size is large enough (e.g., over 64 KB), unlink tail and call sbrk(-(intptr_t)(HDR + tail->size)).

**6. Two blocks are consecutive in your list but not adjacent in memory. How did this happen, and what does your coalescing code do about it?**
Between two calls to grow(), libc's own malloc() (called by functions like printf() in the test harness) invoked sbrk() and claimed a chunk of heap addresses between our two blocks. Before merging curr and curr->next, coalesce() checks (char *)payload(curr) + curr->size == (char *)curr->next; because the addresses do not touch across libc's gap, it skips merging them so libc's memory is never corrupted.

## Known Limitations

- `my_free()` and coalescing walk the whole list, so freeing is O(n). Searching for a free block is also O(n).
- Memory obtained with `sbrk()` is never returned to the OS, even when the last block is free.
- `grow()` does not check for extremely large requests, so a request close to the maximum size could make `sbrk()` receive a negative number and shrink the heap.
- `my_free()` reads the header 32 bytes before the pointer it is given. A completely random pointer could crash before the magic number check runs.
- `my_realloc()` returns NULL for an invalid or already freed pointer without printing an error message.
- The allocator is not thread-safe, because there is no locking around the global list.
- The benchmark uses 50,000 random operations in total (each one a malloc or a free), and the statistics are taken while some blocks are still allocated.

## Sources and AI-Tool Declaration

**Websites and documentation**
- Remzi and Andrea Arpaci-Dusseau, *Operating Systems: Three Easy Pieces*, Chapter 17 "Free-Space Management": https://pages.cs.wisc.edu/~remzi/OSTEP/vm-freespace.pdf
  Used to understand block headers, the free list, splitting, coalescing, and first-fit vs best-fit.
- OSTEP Chapter 14 "Memory API": https://pages.cs.wisc.edu/~remzi/OSTEP/vm-api.pdf
  Used for the expected behaviour of malloc, free, calloc and realloc.
- Dan Luu, "Malloc tutorial": https://danluu.com/malloc-tutorial/
  Used to see how a simple allocator uses sbrk() and a header struct.
- Linux man page for brk/sbrk: https://man7.org/linux/man-pages/man2/brk.2.html
  Used for how sbrk(0) works and the (void *)-1 failure value.
- Linux man page for malloc/calloc/realloc: https://man7.org/linux/man-pages/man3/malloc.3.html
  Used for the standard rules for realloc(NULL, n), realloc(p, 0) and calloc overflow.
- Doug Lea, "A Memory Allocator": http://gee.cs.oswego.edu/dl/html/malloc.html
  Used to understand why glibc's allocator is faster than a single linked list (for REPORT.md).


**Course material**
- ICS 2305 Lecture 3 and 4, "Memory Management" (Karanja Mwangi)
  Used for background on the stack, the heap and dynamic memory allocation.

**AI tools**
- Claude (Anthropic): used to explain the concepts in each task (sbrk, alignment, splitting, coalescing, fragmentation), to review my code and point out bugs and missing checks, to explain compiler and linker errors and to draft the structure of this readme. I wrote and tested the code myself, and fixed the bugs it pointed out.
