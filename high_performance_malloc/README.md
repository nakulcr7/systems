# High-performance thread-safe allocator

Designed a thread-safe free-list based memory allocator that is optimally faster than the system allocator and way faster than a single-threaded malloc with a single mutex.

To test performace, two programs are used:

- `ivec_main` - [Collatz Conjecture](https://en.wikipedia.org/wiki/Collatz_conjecture) simulation with dynamic array

- `list_main` - Collatz Conjecture simulation with linked list

## Usage

Run `make all` and `make test` to build and test.

## Design

- To keep track of free chunks of memory, singly-linked nodes with headers are used.

- There is one free list, from which other lists and bins can get memory. If the free list is empty or can't satisfy any memory request from other lists and bins, 4MB of data is allocated to the list and before procesing other requests.

- I use three singly linked lists for storing chunks of size 16, 24 and 62. This is because, I know ahead of time that requests for these sizes will be very popular: They are sizes of `list`, `ivec`, and `num_tasks`. When someone frees a pointer of this size, I put the block of memory back to its appropriate segregated list. Using segregated lists also avoids the need for coalescing.

- I also have 7 bins containing blocks of sizes `32`, `128`, `256`, `512`, `1024`, `2048`, and `4096`. When a user requests for memory of `4096` bytes or less, the program finds the appropriate bin and gives the user the whole block of memory in that bin. For example, when a user requests for `32` bytes or less of memory, the program will locate to bin containing `32`-byte chunks, and give the user one of those chunks (of exact size `32`, no splitting). When the user frees it, the program sticks it back to the bin. If the bin is empty, I allocate `600` blocks of memory to that bin. Having bins with chunks of exact sizes also avoids the need for coalescing.

- Large requests that request more than `4096` bytes of memory are handled using `mmap` and `munmap`.

- Each arena contains of a free list, the 3 segregated lists and 7 bins. Each arena is thread local to speed up the allocator.

## Results

|         | Par-Ivec | Sys-Ivec | Sim-Ivec | Par-List | Sys-List | Sim-List |
| ------- | -------- | -------- | -------- | -------- | -------- | -------- |
|    1000 |    0.005 |    0.008 | 0.055    |    0.023 |    0.011 | 1.127    |
|    5000 |    0.027 |    0.031 | 1.351    |    0.082 |    0.054 | 48.615   |
|   50000 |    0.137 |    0.196 | 3m37.653 |    0.817 |    0.398 | 5m+      |
|  100000 |    0.294 |    0.356 | 5m+      |    1.632 |    0.720 | 5m+      |
|  500000 |    1.725 |    1.848 | 5m+      |    8.216 |    3.659 | 5m+      |
| 1000000 |    3.923 |    4.361 | 5m+      |   17.909 |    7.477 | 5m+      |

- The allocator is significantly faster than the system when using the list data structure. This is expected because the segregated lists are heavily utilized as the main program requests memory mostly of specific sizes: `16` and `24`.

- The speedup for the collatz ivec program is not as significant as it is for the list program. This is because when an ivec grows, it requests for memory of a different size. The memory in the ivec program varies much more than the list program. Therefore, segregated lists are not as utilized.

- The simple memory allocator is by far slower than the optimized and system allocators. This is expected because the allocator uses mutex to guard the free list, and operations are in order O(n) instead of O(1).

- List is slower than ivec. This is expected because list is implements as a singly linked list, and ivec acts like an array. Accessing elements is easier for ivec, which makes ivec a lot faster.
