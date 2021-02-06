/**
 * @file allocator.c
 *
 * Explores memory management at the C runtime level.
 *
 * Author: Chiara Lim, Marcus Bradlee
 *
 * To use (one specific command):
 * LD_PRELOAD=$(pwd)/allocator.so command
 * ('command' will run with your allocator)
 * vim tests/outputs/03.md 
 * To use (all following commands):
 * export LD_PRELOAD=$(pwd)/allocator.so
 * (Everything after this point will use your custom allocator -- be careful!)
 */

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>

#include "allocator.h"
#include "logger.h"

#define MEM_SIZE sizeof(struct mem_block); 

static struct mem_block *g_head = NULL; /*!< Start (head) of our linked list */
static unsigned long g_allocations = 0; /*!< Allocation counter */
static size_t page_sz = 4096;
static bool is_scribbling = false;

pthread_mutex_t alloc_mutex = PTHREAD_MUTEX_INITIALIZER; /*< Mutex for protecting the linked list */

/**
 * A simple LOG function to LOG contents of a struct mem_block.
 *
 * @param block
 */
void print_block(struct mem_block *block){
    LOG("\t\talloc_id: %lu\n", block->alloc_id);
    LOG("\t\tblock_name: %s\n", block->name);
    LOG("\t\tblock_size: %zu\n", block->size);
    LOG("\t\tblock_usage: %zu\n", block->usage); 
}

/**
 * If the current region has no reusable space, this requests space from the OS 
 * using mmap() and adds the new block to the end of the linked list.
 *
 * @param region_sz
 */
void *request(size_t region_sz){
    LOGP("\t---- REQUEST() ----\n");
    /* note that we are mmaping region_sz */
    void *block = mmap(NULL, region_sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    /* error checking */
    if(block == MAP_FAILED){
        perror("mmap");
        return NULL;
    }
    LOGP("\t[✓] Successfully request() memory.\n");
    return block;
}

/**
 * Populates any given mem_block struct.
 *
 * @param block, requested_sz, block_sz, start
 */
void populate(struct mem_block *block, size_t requested_sz, size_t block_sz, struct mem_block *start){
    LOGP("\t---- POPULATE() ----\n");
    /* each allocation will increment g_allocations by one and represent the alloc_id */
    block->alloc_id = g_allocations++;
    /* naming each block */
    sprintf(block->name, "Allocation %lu", block->alloc_id);
    /* block.size should represent the size of the block */
    block->size = block_sz;
    /* block.usage represents how much of the region is being used by the block, AKA block_sz */
    block->usage = requested_sz;
    /* block.region_start should point to the beginning of the mapped region */
    block->region_start = start;
    /* each new block will be added to the end of the region so next should be NULL */
    block->next = NULL;
    //print_block(block);
    LOGP("\t[✓] Successfully populate() memory\n");
}

/**
 * Creates a block and adds it to linked list by splitting avaliable block
 *
 * @param block, size
 */
void *split(void *block, size_t size){
    /* we can assume the block given to us is big enough */

    LOGP("\t\t---- SPLIT() ----\n");

    struct mem_block *curr = block;
    struct mem_block *new = NULL;

    if( curr->usage == 0 ){
        LOG("\t\tUpdating block (alloc_id): %lu\n", curr->alloc_id);
        /* we just want to update the block's usage */
        curr->usage = size;
        return curr;
    }

    /* SPLITTING THE BLOCK */

    LOG("\t\tSplitting block (alloc_id): %lu\n", curr->alloc_id);
    /* update the curr size */
    size_t new_block_sz = curr->size;
    curr->size = curr->usage;
    new_block_sz -= curr->usage;
    new = (void *)curr + curr->size; /* includes sizeof(struct mem_block) */
    /* populate new mem_block */
    populate(new, size, new_block_sz, curr->region_start);

    /* case where we split in the middle */
    if( curr->next != NULL ){ 
        /* update linked list */
        new->next = curr->next;
    }
    curr->next = new;
    
    LOGP("\t\t[✓] Successfully split() block.\n");
    return new;
}

/**
 * Using the first fit FSM implementation, it reuses free memory in a region
 * by finding the first, suitable memory block.
 *
 * @param size
 */
void *first_fit(size_t size)
{
    LOGP("\t---- FIRST_FIT() ----\n");
    struct mem_block *curr = g_head;
    /* We want to keep searching until we find a block that is free 
    * and large enough */
    while( curr != NULL ){  
        /* case where block is partially free */
        if( (curr->size - curr->usage) >= size ){
            LOG("\t[✓] Found a block! size free: %zu\n", curr->size - curr->usage);
            /* returns a pointer of first half of block to be split */
            return curr;
        }
    
        if( curr->next == NULL){
            LOGP("\t[X] No reusable space\n");
            return NULL;
        }
        curr = curr->next;
    }
    return NULL;
}

/**
 * Using the worst fit FSM implementation, it reuses free memory in a region
 * by finding the largest continuous memory block.
 *
 * @param size
 */
void *worst_fit(size_t size)
{
    LOGP("\t---- WORST_FIT() ----\n");
    struct mem_block *curr = g_head;
    struct mem_block *worst = NULL;
    size_t worst_difference, check_difference;
    
    while( curr != NULL ){
        check_difference = curr->size - curr->usage;
        if( check_difference >= size ){
            if( worst == NULL){
                worst = curr;
                worst_difference = check_difference;
            } else {
                /* it is closer than the other value */
                if( check_difference > worst_difference){
                    worst = curr;
                    worst_difference = check_difference;
                }
            }
        }
        curr = curr->next;
    }

    return worst;
}

/**
 * Using the best fit FSM implementation, it reuses free memory in a region
 * by finding the first closest-in-size memory block.
 *
 * @param size
 */
void *best_fit(size_t size)
{
    LOGP("\t---- BEST_FIT() ----\n");
    struct mem_block *curr = g_head;
    struct mem_block *best = NULL;
    size_t best_difference, check_difference;

    while( curr != NULL ){
        check_difference = curr->size - curr->usage;
        if( (curr->usage == 0 && curr->size == size) || (check_difference == size) ){
            return curr;
        } else if( check_difference > size ){
            if( best == NULL){
                best = curr;
                best_difference = check_difference;
            }
            /* it is closer than the other value */
            if( check_difference < best_difference && check_difference != best_difference){ //breaks tie
                best = curr;
                best_difference = check_difference;
            }

        }
        curr = curr->next;
    }

    if( best == NULL ){
        return NULL;
    }

    return best;
}

/**
 * Using free space management (FSM) algorithms, it finds a block of
 * memory that we can reuse. It returns NULL if no suitable block is found.
 *
 * @param size
 */
void *reuse(size_t size)
{
    LOGP("\t\t---- REUSE() ----\n");

    /* algo determines which FSM we will be using */
    char *algo = getenv("ALLOCATOR_ALGORITHM");
    if (algo == NULL) {
        algo = "first_fit";
    }

    void *ptr = NULL;
    if (strcmp(algo, "first_fit") == 0) {
        ptr = first_fit(size);
    } else if (strcmp(algo, "best_fit") == 0) {
        ptr = best_fit(size);
    } else if (strcmp(algo, "worst_fit") == 0) {
        ptr = worst_fit(size);
    } else {
        return NULL;
    }

    if(ptr != NULL){
        ptr = split(ptr, size);
    }

    return ptr;
}

/**
 * Allocates memory by checking if you can reuse an existing block. 
 * If not, it maps a new memory region.
 * 
 * @param size
 */
void *malloc(size_t size)
{
    LOGP("\t---- MALLOC() ----\n");

    pthread_mutex_lock(&alloc_mutex);
    LOGP("\t[🔒] pthread locked\n");

    if(size <= 0){
        pthread_mutex_unlock(&alloc_mutex);
        return NULL;
    }

    size += sizeof(struct mem_block);

    /* resize size to be aligned to 8 bytes */
    if(size % 8 != 0){
        size = size + ( 8 - size % 8);
    }

    /* create the size of the block, which includes the struct 
     * at the beginning of the block. block_sz represents how 
     * much of the region is being used (usage) */
    size_t block_sz = size;
    /* for every page_sz bytes we need one page, so anything 
     * less than page_sz bytes should still result in 1 page */
    size_t num_pages = block_sz / page_sz;
    if( (block_sz % page_sz) != 0 ){
        num_pages += 1;
    }
    /* the total size of the region we are storing blocks in 
     * will be the num_pages * page_sz */
    size_t region_sz = num_pages * page_sz;

    LOG("\t\tBlock size: %zu bytes\n", block_sz);
    LOG("\t\tsize: %zu bytes\n", region_sz);

    /* this is the very first implementation: 0 regions, 0 blocks */
    struct mem_block *block = NULL;
    if(g_head == NULL){
        LOGP("\t\tThis is the first malloc() call.\n");

        /* CHECK SCRIBBLING */ 
        char *scribble = getenv("ALLOCATOR_SCRIBBLE");
        LOG("\t[✍️] Scribble: %s\n", scribble);
        if( scribble != NULL && atoi(scribble) == 1 ){
            LOGP("\t[✍️] Scribbling Mode ON\n");
            is_scribbling = true;
        }

        /* requesting space */
        block = (struct mem_block *) request(region_sz);
        /* check if region was created */
        
        if(block == NULL){
            perror("request");
            pthread_mutex_unlock(&alloc_mutex);
            LOGP("\t[🔑] pthread unlocked\n");
            return NULL;
        }

        /* populate the mem_block */
        populate(block, size, region_sz, block);
        block->region_size = region_sz;
        /* set new head */
        g_head = block;

        if( is_scribbling ){
            LOGP("\t[✍️] Trying to scribble 0xAA\n");
            size_t scrib_sz = block->size - sizeof(struct mem_block);   
            memset(block + 1, 0xAA, scrib_sz); 
            is_scribbling = false;
            LOGP("\t[✍️] Done!\n");
        }

    } else {
        LOGP("\tChecking for reuse...\n");

        /* CHECK SCRIBBLING */ 
        char *scribble = getenv("ALLOCATOR_SCRIBBLE");
        LOG("\t[✍️] Scribble: %s\n", scribble);
        if( scribble != NULL && atoi(scribble) == 1 ){
            LOGP("\t[✍️] Scribbling Mode ON\n");
            is_scribbling = true;
        }

        /* we want to see if we can reuse any space */
        block = (struct mem_block *) reuse(size);
        /* check if there is any reusable space */
        if(block == NULL){
            LOGP("\tCreating new region...\n");
            /* there is no reusable space so we need to create a new region */
            block = (struct mem_block *) request(region_sz);
            
            /* check if region was created */
            if(block == NULL){
                perror("request"); 
                pthread_mutex_unlock(&alloc_mutex);
                LOGP("\t[🔑] pthread unlocked\n");
                return NULL;
            }
            /* populate the mem_block */
            populate(block, size, region_sz, block);
            block->region_size = region_sz;
            /* update linked list */
            struct mem_block *curr = g_head;
            while(curr->next != NULL){
                curr = curr->next;
            }
            curr->next = block;
        }

        if( is_scribbling ){
            LOGP("\t[✍️] Trying to scribble 0xAA\n");
            size_t scrib_sz = block->size - sizeof(struct mem_block);   
            memset(block + 1, 0xAA, scrib_sz); 
            is_scribbling = false;
            LOGP("\t[✍️] Done!\n");
        }
    }

    /* RETURN POINTER */
    pthread_mutex_unlock(&alloc_mutex);
    LOGP("\t[🔑] pthread unlocked\n");

    /* returns block + 1 because block is pointing to the struct header not the data... I think */
    LOGP("\t[✓] Successfully malloc() memory.\n\n");
    return block + 1;
}

/**
 * It is a version of malloc that allows customized memory block names.
 *
 * @param size, name
 */
void *malloc_name(size_t size, char *name)
{
    LOGP("\t---- MALLOC_NAME() ----\n");

    struct mem_block *block = (struct mem_block *) malloc(size) - 1;
    
    /* rename the block */
    sprintf(block->name,"%s", name);
    LOG("\tName: %s\n", block->name);
    LOGP("\t[✓] Succesfully malloc_name()\n");
    
    return block + 1;
}

/**
 * Deallocates/frees memory by resetting a block's usage to 0.
 * If an entire region's block usage is 0, it unmaps the memory region.
 *
 * @param ptr
 */
void free(void *ptr)
{   
    LOGP("\t---- FREE() ----\n"); 
    pthread_mutex_lock(&alloc_mutex);
    LOGP("\t[🔒] pthread locked\n");

    if (ptr == NULL) {
        /* Freeing a NULL pointer does nothing */
        LOGP("\t[X] NULL ptr was passed.\n");
        pthread_mutex_unlock(&alloc_mutex);
        return;
    }

    /* set that block's usage to zero */
    struct mem_block *block = (struct mem_block *) ptr - 1;
    LOG("\t\tFreeing alloc id: %lu\n", block->alloc_id);
    block->usage = 0;
    LOGP("\t\tAfter freeing:\n");
    print_block(block);

    /* CHECKING FOR EMPTY REGION */-

    /* Cases:
     * 1. single region (reset head)
     * 2. multiple regions, free first (reset head)
     * 3. two regions, free second (connect head region to null)
     * 4. multiple regions, free middle (connect prev region to next)
    */

    bool region_empty = true;
    bool reset_head = false;

    struct mem_block *start = block->region_start;
    struct mem_block *curr = start;

    /* if the start is the head, we will have to reset the head */
    if(start == g_head){
        reset_head = true;
    }

    /* loop while the region starts are equal or the curr has reached null (case 1) */
    while( curr->region_start == start ){
        /* if a block usage is not 0, the region is not empty */
        if( curr->usage != 0 ){
            region_empty = false;
            break;
        } else if( curr->next == NULL ){
            curr = NULL;
            break;
        } else {
            curr = curr->next;
        }
    }

    /* At this point, curr points to either null or the first block of the adjecent region */
    if( region_empty ){
        if( reset_head ){
            LOGP("\tResetting head...\n");
            /* cases 1 & 2: if the head is being reset, all we have to do is set the head 
            to curr which could be null or another region, no connecting needed */
            g_head = curr;
            size_t empty_size = start->region_size;
            int ret = munmap(start, empty_size);
            if( ret == -1 ){
                perror("munmap");
                pthread_mutex_unlock(&alloc_mutex);
                LOGP("\t[🔑] pthread unlocked\n");
                return;
            }
            LOGP("\t[✓] Region has been unmapped\n");
        } else {
            /* cases 3 & 4: if the head is not being freed, we need to connect the previous region 
            to either null (case 3) or the next region (case 4). Either way we have to loop through
            all previous blocks */
            struct mem_block *prev = g_head;
    
            while(prev->next != start){
                prev = prev->next;
            }
            /* connecting previous region to either null or next block */
            prev->next = curr;

            size_t empty_size = start->region_size;
            int ret = munmap((void *)start, empty_size);
            if(ret == -1){
                perror("munmap");
                pthread_mutex_unlock(&alloc_mutex);
                LOGP("\t[🔑] pthread unlocked\n");
                return;
            }
            LOGP("\t[✓] Region has been unmapped\n");
        }
    }
    pthread_mutex_unlock(&alloc_mutex);
    LOGP("\t[🔑] pthread unlocked\n\n");
    LOGP("\t[✓] Succesfully free()\n");
}

/**
 * Allocates initialized memory space.
 *
 * @param nmemb, size
 */
void *calloc(size_t nmemb, size_t size)
{
    LOGP("\t---- CALLOC() ----\n");

    LOG("\t\tCalloc request: %zu members of size %zu bytes\n", nmemb, size);

    /* malloc with the number of members * size of members */
    void *ptr = malloc(nmemb * size);
    /* use memset to initialize everything to 0 */
    memset(ptr, 0x00, nmemb * size);

    /* return the pointer to the callocd memory (region?/block?) */
    LOG("\t[✓] Successful calloc() memory to %p\n\n", ptr);
    
    return ptr;
}

/**
 * Resizes a region of memory by creating a new memory block 
 * and copying the old memory into the new memory block.
 *
 * @param ptr, size
 */
void *realloc(void *ptr, size_t size)
{
    LOGP("\t---- REALLOC() -------------------------------\n");
    pthread_mutex_lock(&alloc_mutex);
    LOGP("\t[🔒] pthread locked\n");

    size_t check_size = size + sizeof(struct mem_block);
    if(check_size % 8 != 0){
        check_size = check_size + ( 8 - check_size % 8);
    }


    if( ptr == NULL ){
        /* If the pointer is NULL, then we simply malloc a new block */
        pthread_mutex_unlock(&alloc_mutex);
        LOGP("\t[🔑] pthread unlocked\n");
        return malloc(size);
    }

    if( size == 0 ) {
        /* Realloc to 0 is often the same as freeing the memory block... But the
         * C standard doesn't require this. We will free the block and return
         * NULL here. */
        pthread_mutex_unlock(&alloc_mutex);
        LOGP("\t[🔑] pthread unlocked\n");
        free(ptr);
        return NULL;
    }

    struct mem_block* curr = (struct mem_block*)ptr - 1;
    //if( curr->size >= size)
    if( curr->size >= check_size ){
        /* Size provided is too small to realloc */
        pthread_mutex_unlock(&alloc_mutex);
        LOGP("\t[🔑] pthread unlocked\n");
        curr->usage = check_size;
        return ptr;
    }

    /* Time to realloc by malloc-ing new space and free old space.
     * Then, we will copy the old data into the new space. */
    pthread_mutex_unlock(&alloc_mutex);
    LOGP("\t[🔑] pthread unlocked\n");
    void *new_ptr = malloc(size);
    if( !new_ptr ){
        perror("malloc");
        return NULL;
    }
    memcpy(new_ptr, ptr, curr->usage); 
    free(ptr); 

    LOG("\t[✓]Successfully realloc() memory to %p\n\n", new_ptr);
    return new_ptr;
}

/**
 * Prints out the current memory state, including both the regions and blocks.
 * Entries are printed in order, so there is an implied link from the topmost
 * entry to the next, and so on. It also takes in a file pointer to redirect
 * output as requested. 
 *
 * @param fp
 */
void write_memory(FILE* fp){

    LOGP("\t---- WRITE_MEMORY() ----\n");

    //fputs("-- Current Memory State --");
    struct mem_block *current_block = g_head;
    struct mem_block *current_region = NULL;
    while (current_block != NULL) {
        
        if (current_block->region_start != current_region) {
            current_region = current_block->region_start;
            LOGP("\tPrinting region information...\n");
            /* SPRINTF + FPUTS SECTION */
            /* sprintf to stderr in function: write_memory() */

            fprintf(fp, "[REGION] %p-%p %zu\n",
                    current_region,
                    (void *) current_region + current_region->region_size,
                    current_region->region_size);
        }
        LOGP("\tPrinting block information...\n");
        fprintf(fp, "[BLOCK]  %p-%p (%lu) '%s' %zu %zu %zu\n",
                current_block,
                (void *) current_block + current_block->size,
                current_block->alloc_id,
                current_block->name,
                current_block->size,
                current_block->usage,
                current_block->usage == 0
                    ? 0 : current_block->usage - sizeof(struct mem_block));
        current_block = current_block->next;
    }
}

/**
 * Calls write_memory() with a default file pointer to stdout.
 *
 * @param void
 */
void print_memory(void)
{
    write_memory(stdout);
}