# Task 8: Allocator Benchmark & Fragmentation Report

## 1. Benchmark Configuration
- **Workload:** 50,000 randomized `malloc`/`free` operations across 512 active pointer slots
- **Allocation sizes:** Uniformly distributed from `1` to `4096` bytes
- **PRNG seed:** `srand(42)` (identical operation sequence across all three runs)
- **Timing:** `clock_gettime(CLOCK_MONOTONIC, ...)`

## 2. Measurements

| Policy / Allocator | Time (ms) | `heap_bytes` | `free_bytes` | `free_blocks` | `largest_free` | External Fragmentation (`1 - largest_free / free_bytes`) |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| `FIT_FIRST` | 85.01 | 720896 | 152128 | 118 | 67040 | 0.5593 |
| `FIT_BEST` | 116.86 | 720896 | 152912 | 80 | 77200 | 0.4951 |
| `libc` (`malloc`/`free`) | 2.99 | — | — | — | — | — |


## 3. Policy Choice and Comparison with `glibc`

For me I would choose FIT_FIRST over FIT_BEST.This is because in FIT_FIRST it uses less bytes since after computation it has more free bytes compared to the other ones. While FIT_BEST reduces external fragmentation slightly by packing allocations into the tightest available hole and leaving smaller splinters, it must scan the entire linked list on every allocation. FIT_FIRST stops as soon as it finds the first fitting block, trading a minor increase in fragmentation for noticeably faster allocation latency as block count grows.

Compared to `glibc`'s `ptmalloc2`, the allocator is both slower and less space-efficient. The design scans a single linear list of both allocated and free blocks and carries a 32-byte header per block, whereas `glibc` segregates free chunks into size-indexed bins (fastbins and small bins) , uses compact boundary tags, and avoids `sbrk()` contention which may be used to grow the heap ,