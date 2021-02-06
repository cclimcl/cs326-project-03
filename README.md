# Project 3: Memory Allocator

See: https://www.cs.usfca.edu/~mmalensek/cs326/assignments/project-3.html 

To compile and use the allocator:

```bash
make
LD_PRELOAD=$(pwd)/allocator.so ls /
```

(in this example, the command `ls /` is run with the custom memory allocator instead of the default).

## Testing

To execute the test cases, use `make test`. To pull in updated test cases, run `make testupdate`. You can also run a specific test case instead of all of them:

```
# Run all test cases:
make test

# Run a specific test case:
make test run=4

# Run a few specific test cases (4, 8, and 12 in this case):
make test run='4 8 12'
```

General Purpose:

This program is a custom memory allocator that uses systems calls and free space managment alogrithms (FSM) to allocate and deallocate memory.

Main Memory Functions:

	(A) void *malloc(size_t size);

		This function is a version of the linux malloc() that allows allocations of memory blocks. The gerneral 
		structure of function is:
		
		(1) Align the requested size which includes the size of struct mem_block
		(2) If this is the very first call to malloc(), create a new region using request()
		(3) Else, using reuse(), check to see if there is an 'free' space in the already mapped regions
		(4) If so, populate the reusable space, and if not, request() a new region.


		The function malloc_name() allows for users to customize names of the requested memory block.

	(B) void free(void *ptr); [CASE 2 - free]

		This function is a version of the linux free() that deallocates/frees memory by resetting a block's usage to 0.
		If an entire region's block usage is 0, it unmaps the memory region using munmap(). The gerneral structure of function is:

		(1) Set ptr's struct mem_block usage to 0.
		(2) Check to see it the block's region has memory blocks of all usage 0. If so, unmap the region and reset the head if necessary.

	(C) void *calloc(size_t nmemb, size_t size);

		Similar to malloc(), this function allocates memory space by calling malloc() and uses memset() to initialize the memory block.
		The gerneral structure of function is:

		(1) Allocate memory using malloc()
		(2) Initialize block using memset()

	(D) void *realloc(void *ptr, size_t size); [CASE 11 - Unix Utilities]

		This function is a version of the linux realloc() that resizes a region of memory by creating a new memory block and copying the old memory into the new memory block. The gerneral structure of function is:

		(1) Check if the current block size is big enough to handle the request. If so, just update the block's usage.
		(2) Else, create a new memory block using malloc() and copy over the current block's information using memcpy().

Helper Functions:

	(A) void *request(size_t region_sz);

		This function requests space from the OS using mmap() to create a new region. It then adds the new block to the end of the memory linked list. The gerneral structure of function is:

	(B) void populate(struct mem_block *block, size_t requested_sz, size_t block_sz, struct mem_block *start);

		Various functions call this to populate a newly allocated block. It updates the block's memory struct accordingly.

	(C) void *reuse(size_t size);

		This function determines with FSM alogrithm to use usign getenv() and then calls the different FSM functions accordingly.

	(D) void *split(void *block, size_t size);

		This function is used by the FSM functions to either update information of a given block or split the block to accommodate the reuse of memory in a region. The gerneral structure of function is:

		(1) Check if the current block's usage is 0. If so, just update the block's usage the accomodate the size.
		(2) Else, we split the block such that the second half of the current block becomes a new memory block which can be reused.

	(E) void write_memory(FILE* fp);

		This function prints out the current memory state, including both the regions and blocks. Entries are printed in order, so there is an implied link from the topmost entry to the next, and so on. It also takes in a file pointer to redirect output as requested. 

	FSM Algorithms: [CASES 3 - 8 - (basic) first, worst, best fit]

	(F) void *first_fit(size_t size);

		This function makes use of the first fit FSM implementation, to reuses free memory in a region by finding the first free, suitable memory block.

	(G) void *worst_fit(size_t size);

		This function makes use of the worst fit FSM implementation, to reuses free memory in a region by finding the largest continuous memory block.

	(H) void *best_fit(size_t size);

		This function makes use of the best fit FSM implementation, to reuses free memory in a region by finding the first closest-in-size memory block.

	Test Cases Review:

	(A) Scribbling

		In malloc(), if the ALLOCATOR_SCRIBBLE environment variable is set to 1, it fills the new allocation with 0xAA (10101010 in binary). This means that if a program assumes memory allocated by malloc is zeroed out, it will be in for a rude awakening.
		The general structure is:

		(1) Check ALLOCATOR_SCRIBBLE environment with getenv(). If variable is not NULL and is 1, scribbling mode is on.
		(2) After calling reuse() or request(), if scribbling mode is on, use memset to scribble the memory block.

	(B) Thread Safety

		If malloc is called multiple times rapidly, some malloc() might be overwritten and thus, by using thread_mutex_lock() and thread_mutex_unlock(), we lock threads such that different threads do not over right each other. We would then lock alloc_mutex before each function and unlock if before returning.


![](giphy.gif)
