/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* bu username : eg. jappavoo */
    "cclim",
    /* full name : eg. jonathan appavoo */
    "chiara lim",
    /* email address : jappavoo@bu.edu */
    "cclim@bu.edu",
    "",
    ""
};

/* --------- HELPER FUNCTIONS FROM DISCUSSION #6 -------------- */

/* single word (4) or double word (8) alignment */
#define ALIGNMENT   8
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE   4 /* Word size: word, header, and footer size (btyes) */
#define DSIZE   8 /* Double word size */
#define CHUNKSIZE   (1 << 12) /* Extends the heap by this size */

/* Packs a size and allocated bit into a word */
#define PACK(size, alloc)   ((size) | (alloc)) 

/* Read and write a word at address p */
#define GET(ptr)        (*(unsigned int *) (ptr))      
#define PUT(ptr, value) (*(unsigned int *) (ptr) = (value))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     (GET (p) & ~0x7)
#define GET_ALLOC(p)    (GET (p) & 0x1) /* if a = 1: allocated block */

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)    ((char *)(bp) - WSIZE)  
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* ------------------------------------------------------------------------------------ */
typedef struct mem_block {
    size_t size;
    char *header;
    char *footer;
    struct mem_block *next;
} mem_block;

static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *best_fit(size_t size);
static void split(void *bp, size_t newsize);

// static struct mem_block *heap_listp = NULL; /*!< Start (head) of our linked list */
static void *heap_listp = 0; /*!< Start (head) of our linked list */

/* 
 * mm_init - initialize the malloc package.
 * Allocates the initial heap area
 * Returns -1 if there are problmes with initializing,
 * 0 otherwise.
 */
int mm_init(void)
{   /* Creates the initial empty heap memory block */
    heap_listp = mem_sbrk(4 * WSIZE);
    /* Error checking */
    if (heap_listp == (void *)-1){
        return -1;
    }
    
    PUT(heap_listp, 0);                                 /* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));      /* Prologue header */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));      /* Prologue footer */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));          /* Epilogue header */
    heap_listp += (2 * WSIZE);
    /* Extends the rest of the heap with one free memory block
     * of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL){
        return -1;
    }
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    void *bp;

    /* Align memory block size */
    int newsize = ALIGN(size + SIZE_T_SIZE);
    
    /* Best fit search for request memory size.
     * Case 1: best_fit() finds a space to add 
     * Case 2: best_fit() returns NULL and thus, we must request more memory
     */
    bp = best_fit(newsize);
    if (bp != NULL) {
        split(bp, newsize);
        return (bp);
    } 
    /* we need to extend the size */
    bp = mem_sbrk(newsize);
    if (bp == (void *)-1){
        return NULL;
    } else {
        *(size_t *)bp = size;
        return (void *)((char *)bp + SIZE_T_SIZE);
    }

    // int newsize = ALIGN(size + SIZE_T_SIZE);
    // void *p = mem_sbrk(newsize);
    // if (p == (void *)-1)
    // return NULL;
    // else {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE);
    // }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{

}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

/*
 * coalesce - merges free blocks
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    /* Case 1:  */
    if (prev_alloc && next_alloc) {
        return bp;
    }
    /* Case 2 */
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    /* Case 3 */
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    /* Case 4 */
    else {                                    
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

/* 
 * extend_heap - extends the heap with a free block
 */
static void *extend_heap(size_t words) 
{
    char *bp;
    size_t size;
    
    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    /* There is not enough space */
    if ((long) (bp = mem_sbrk(size)) == -1) 
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* new epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/*
 *
 */
static void *best_fit(size_t size){
    void *bp = heap_listp;
    void *best = NULL;
    size_t size_difference, smallest_difference;

    while (bp != NULL){
        size_difference = GET_SIZE(HDRP(bp)) - size;
        /* we found an exact fit and its free */
        if( size_difference == 0 && GET_ALLOC(bp) == 0 ){
            return bp;
        /* the request size can fit */
        } else if( size_difference  > 0 && GET_ALLOC(bp) == 0 ){
            if( best == NULL ){
                best = bp;
                smallest_difference = size_difference;
            }
            /* it is closer than the other value & accounts for a tie */
            else if ( size_difference < smallest_difference && size_difference != smallest_difference){ 
                best = bp;
                smallest_difference = size_difference;
            }

        }
        bp = NEXT_BLKP(bp); // [Not sure if this is how you move to the next pointer]
    }
    return bp;
 }

static void split(void *bp, size_t newsize)
{
    size_t leftover = GET_SIZE(bp) - newsize; 

    /* Update size of current block */
    PUT(HDRP(bp), PACK(newsize, 1));
    PUT(FTRP(bp), PACK(newsize, 1));

    if (leftover > 0){
        bp = NEXT_BLKP(bp);
        /* Place information in new block */
        PUT(HDRP(bp), PACK(leftover, 0));
        PUT(FTRP(bp), PACK(leftover, 0));
    }
}