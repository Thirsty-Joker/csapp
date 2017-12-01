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
team_t team =
{
    /* Team name */
    "1",
    /* First member's full name */
    "zerods",
    /* First member's email address */
    "xx",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */
#define DSIZE       8       /* Doubleword size (bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y)? (x) : (y))  

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))            
#define PUT(p, val)  (*(unsigned int *)(p) = (val))   

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)                
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute block size of its next and prev block */
#define GET_NEXT_SIZE(bp) (GET_SIZE(HDRP(NEXT_BLKP(bp))))       
#define GET_PREV_SIZE(bp) (GET_SIZE(HDRP(PREV_BLKP(bp))))       

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)                      
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Given block ptr bp, find next and previous free blocks*/
#define PREV_LINKNODE_RP(bp) ((char *) bp)
#define NEXT_LINKNODE_RP(bp) ((char *) bp + WSIZE)

#define MIN_BLOCK_SIZE 2

#define MAX_SIZE_FOR_MIN_CLASS (1<<4)
#define MAX_SIZE_FOR_MAX_CLASS (1<<12)

/* Global variables */
static char *heap_listp = NULL;  
static char *free_block_list_start = NULL;

/* $end mallocmacros */

/* Function prototypes for internal helper routines */
static void *extend_heap(size_t dwords);
static void *coalesce(void *bp);
static void *find_fit(size_t size);
static void place(void *bp, size_t asize);
static void reallocPlace(void *bp, size_t asize);

static char *find_list_root(size_t size);
static void deleteFromList(char *p);
static void insertToList(char *p);
static void *realloc_coalesce(void *bp,size_t newSize,int *isNextFree);
static void realloc_place(void *bp,size_t asize);

int mm_check(char *function);
/*
 * mm_init - initialize the malloc package.
 * The return value should be -1 if there was a problem in performing the initialization, 0 otherwise
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    // Allocate space for blocks and the headers of segregated lists
    if ((heap_listp = mem_sbrk(14*WSIZE)) == (void *) -1) {
        return -1;
    }

    PUT(heap_listp, 0);                 /*block size list<=16*/
    PUT(heap_listp + (1*WSIZE), 0);     /*block size list<=32*/
    PUT(heap_listp + (2*WSIZE), 0);     /*block size list<=64*/
    PUT(heap_listp + (3*WSIZE), 0);     /*block size list<=128*/
    PUT(heap_listp + (4*WSIZE), 0);     /*block size list<=256*/
    PUT(heap_listp + (5*WSIZE), 0);     /*block size list<=512*/
    PUT(heap_listp + (6*WSIZE), 0);     /*block size list<=1024*/
    PUT(heap_listp + (7*WSIZE), 0);     /*block size list<=2048*/
    PUT(heap_listp + (8*WSIZE), 0);     /*block size list<=4096*/
    PUT(heap_listp + (9*WSIZE), 0);     /*block size list >4096*/
    PUT(heap_listp + (10*WSIZE), 0);
    PUT(heap_listp + (11*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (12*WSIZE), PACK(DSIZE,1));
    PUT(heap_listp + (13*WSIZE), PACK(0, 1));

    free_block_list_start = heap_listp;
    heap_listp += (12*WSIZE);

    if ((extend_heap(CHUNKSIZE/DSIZE)) == NULL) {
        return -1;
    }

    return 0;
}

/* 
 * extend_heap - Extend heap with free block and 
 * return its block pointer 
 * 4-word aligned
 */
static void *extend_heap(size_t dwords)
{
    char *bp;
    size_t size;

    size = (dwords % 2) ? (dwords+1) * DSIZE : dwords * DSIZE;

    if ((long) (bp = mem_sbrk(size)) == (void *) -1) {
        return NULL;
    }

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(NEXT_LINKNODE_RP(bp), NULL);
    PUT(PREV_LINKNODE_RP(bp), NULL);

    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    /* Ignore bad requests */
    if (size ==0) {
        return NULL;
    }

    /* Minimum block size */
    if (size <= DSIZE) {
        asize = 2*(DSIZE);
    }
    /* Adjust block size to include overhead and alignment reqs. */ 
    else {
        asize = (DSIZE) * ((size+(DSIZE)+(DSIZE-1)) / (DSIZE));
    }

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/DSIZE)) == NULL) {
        return NULL;
    }
    place(bp,asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    if (bp == 0) {
        return;
    }
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(NEXT_LINKNODE_RP(bp), NULL);
    PUT(PREV_LINKNODE_RP(bp), NULL);
    coalesce(bp);
}

/*coalesce the empty block*/
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    /*coalesce the block and change the point*/
    if (prev_alloc && next_alloc) {                 /* Case 1 Allocated both do nothing */
    }
    else if (prev_alloc && !next_alloc) {           /* Case 2 Merge next free block */
        size += GET_NEXT_SIZE(bp);
        deleteFromList(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size,0));
        PUT(FTRP(bp), PACK(size,0));
    }   
    else if(!prev_alloc && next_alloc){             /* Case 3 Merge previous free block */
        size += GET_PREV_SIZE(bp);
        deleteFromList(PREV_BLKP(bp));
        PUT(FTRP(bp), PACK(size,0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp);

    }
    else {                                          /* Case 4 Merge both */
        size += GET_PREV_SIZE(bp) + GET_NEXT_SIZE(bp);
        deleteFromList(PREV_BLKP(bp));
        deleteFromList(NEXT_BLKP(bp));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp);
    }
    insertToList(bp);
    return bp;
}

/**
 *  Given a size, return the proper list pointer of FreeList ptr array
 */
static inline char *find_list_root(size_t size)
{
    int index = 0;

    for (int blockSize = MAX_SIZE_FOR_MIN_CLASS; 
            blockSize <= MAX_SIZE_FOR_MAX_CLASS; blockSize <<= 1) {
        if(size <= blockSize) {
            return free_block_list_start+(index*WSIZE);       
        }
        index++;
    }

    return free_block_list_start+(index*WSIZE);
}

/*
 * static char **freeListArray = (char **) heap_listp;
 *
 * Delete free block bp from list, find index first.
 */
static inline void deleteFromList(char *p)
{
    char *root = find_list_root(GET_SIZE(HDRP(p)));
    char *prevp = GET(PREV_LINKNODE_RP(p));
    char *nextp = GET(NEXT_LINKNODE_RP(p));

    if (prevp == NULL) {
        if(nextp != NULL) {
            PUT(PREV_LINKNODE_RP(nextp), NULL);
        }
        PUT(root,nextp);
    }
    else {
        if (nextp != NULL) {
            PUT(PREV_LINKNODE_RP(nextp), prevp);
        }
        PUT(NEXT_LINKNODE_RP(prevp), nextp);
    }

    PUT(NEXT_LINKNODE_RP(p), NULL);
    PUT(PREV_LINKNODE_RP(p), NULL);
}

/*
 * Insert new free block to segregated free list
 * best-fit policy --- insert and find sequentially
 */
static inline void insertToList(char *p)
{
    char *root = find_list_root(GET_SIZE(HDRP(p)));
    char *prevp = root;
    char *nextp = GET(root);

    // find the position to insert the block
    while (nextp != NULL) {
        if (GET_SIZE(HDRP(nextp)) >= GET_SIZE(HDRP(p))) {
            break;
        }
        prevp = nextp;
        nextp = GET(NEXT_LINKNODE_RP(nextp));
    }
    // root node
    if (prevp == root) {
        PUT(root, p);
        PUT(NEXT_LINKNODE_RP(p), nextp);
        PUT(PREV_LINKNODE_RP(p), NULL);
        if (nextp != NULL) {
            PUT(PREV_LINKNODE_RP(nextp), p);
        }
    }
    else {
        PUT(NEXT_LINKNODE_RP(prevp), p);
        PUT(PREV_LINKNODE_RP(p), prevp);
        PUT(NEXT_LINKNODE_RP(p), nextp);
        if (nextp != NULL) {
            PUT(PREV_LINKNODE_RP(nextp), p);
        }
    }

}

/* 
 * find_fit - Find a fit for a block with asize bytes 
 */
static void *find_fit(size_t size)
{
    /*first fit*/
    char *root = find_list_root(size);
    for(; root != (heap_listp-(2*WSIZE)); root += WSIZE) {
        char *tmpP = GET(root);
        while (tmpP != NULL) {
            if(GET_SIZE(HDRP(tmpP)) >= size) {
                return tmpP;
            }
            tmpP = GET(NEXT_LINKNODE_RP(tmpP));
        }
    }
    return NULL;
}

/* 
 * place - Place block of asize bytes at start of free block bp 
 *         and split if remainder would be at least minimum block size
 */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    deleteFromList(bp);

    if ((csize-asize) >= (MIN_BLOCK_SIZE*DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);

        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        PUT(NEXT_LINKNODE_RP(bp), 0);
        PUT(PREV_LINKNODE_RP(bp), 0);
        coalesce(bp);
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

static void reallocPlace(void *bp, size_t size)
{
   size_t csize = GET_SIZE(HDRP(bp));

   PUT(HDRP(bp), PACK(csize, 1));
   PUT(FTRP(bp), PACK(csize, 1));
}

/*
 * mm_realloc
 */
void *mm_realloc(void *ptr, size_t size)
{
    size_t oldsize = GET_SIZE(HDRP(ptr));
    void *newptr;
    size_t asize;

    // size == 0, just free the block
    if (size == 0) {
        mm_free(ptr);
        return 0;
    }

    // ptr == NULL, just malloc the size space
    if (ptr == NULL) {
        return mm_malloc(size);
    }

    if (size <= DSIZE) {
        asize = 2 * (DSIZE);
    } 
    else {
        asize = (DSIZE) * ((size+(DSIZE)+(DSIZE-1)) / (DSIZE));
    }

    if (oldsize == asize) {
        return ptr;
    }
    else if (oldsize > asize) {
        reallocPlace(ptr, asize);
        return ptr;
    }
    else{
        int isNextFree;
        char *bp = realloc_coalesce(ptr, asize, &isNextFree);
        if ( isNextFree == 1) {
            reallocPlace(bp, asize);
            return bp;
        } 
        else if (isNextFree == 0 && bp != ptr){
            memcpy(bp, ptr, size);
            reallocPlace(bp, asize);
            return bp;
        }
        else {
            newptr = mm_malloc(size);
            memcpy(newptr, ptr, size);
            mm_free(ptr);
            return newptr;
        }
    }
}



static void *realloc_coalesce(void *bp, size_t newSize, int *isNextFree)
{
    size_t  prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t  next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    *isNextFree = 0;

    /*coalesce the block and change the point*/
    if (prev_alloc && next_alloc) {
    }
    else if(prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        if(size>=newSize)
        {
            deleteFromList(NEXT_BLKP(bp));
            PUT(HDRP(bp), PACK(size,1));
            PUT(FTRP(bp), PACK(size,1));
            *isNextFree = 1;
            return bp;
        }
    }
    else if(!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        if(size>=newSize)
        {
            deleteFromList(PREV_BLKP(bp));
            PUT(FTRP(bp),PACK(size,1));
            PUT(HDRP(PREV_BLKP(bp)),PACK(size,1));
            bp = PREV_BLKP(bp);
            return bp;
        }

    }
    else
    {
        size +=GET_SIZE(FTRP(NEXT_BLKP(bp)))+ GET_SIZE(HDRP(PREV_BLKP(bp)));
        if(size>=newSize)
        {
            deleteFromList(PREV_BLKP(bp));
            deleteFromList(NEXT_BLKP(bp));
            PUT(FTRP(NEXT_BLKP(bp)),PACK(size,1));
            PUT(HDRP(PREV_BLKP(bp)),PACK(size,1));
            bp = PREV_BLKP(bp);
        }

    }
    return bp;
}


int mm_check(char *function)
{

}



