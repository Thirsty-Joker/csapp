/*
 * mm_explicit.c - The fastest, least memory-efficient malloc package.
 * 
 * In this approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are headers and
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
    /* Team name */
    "ateam",
    /* First member's full name */
    "zerods",
    /* First member's email address */
    "zerods@gmail.com",
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

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)			/* Extend heap by this amount(bytes) */

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))
#define PACK_TWO(size, prev, alloc) ((size) | (prev << 1) (alloc))

#define GET(p) (*(unsigned int *) (p))
#define PUT(p, val) (*(unsigned int *) (p) = (val))

/* Get size of the block in address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
/* Return whether or not the block in addr p is allocated or free */
#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_PREV_ALLOC(p) ((GET(p) & 0x2) >> 1)

#define HDRP(bp) ((char *) (bp) - WSIZE)
#define FTRP(bp) ((char *) (bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *) (bp) + GET_SIZE(((char *) (bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *) (bp) - GET_SIZE(((char *) (bp) - DSIZE)))

/* define free block linklist */
#define PREV_LINKNODE_RP(bp) ((char *) bp)
#define NEXT_LINKNODE_RP(bp) ((char *) bp + WSIZE)

// #define ACCEPT_FIT_SIZE_RANGE 1024

/* function declaration begin */
int mm_checkheap(int verbose);
// static void checkblock(void *bp);

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t size);
static void place(void *bp, size_t asize);
static void update_linkedlist(char* bp);
static void insert_linkedlist(char* bp);
/* function declaration end */

/* global variable begin */
static char *heap_listp = NULL;
static char *root = NULL;
/* global variable end */

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	if ((heap_listp = mem_sbrk(6 * WSIZE)) == (void *) -1) {
		return -1;
	}
	PUT(heap_listp, 0);
	PUT(heap_listp + (1 * WSIZE), 0);
	PUT(heap_listp + (2 * WSIZE), 0);
	PUT(heap_listp + (3 * WSIZE), PACK(DSIZE, 1));
	PUT(heap_listp + (4 * WSIZE), PACK(DSIZE, 1));
	PUT(heap_listp + (5 * WSIZE), PACK(0, 1));

	root = heap_listp + (1 * WSIZE);
	heap_listp += (4 * WSIZE);

	if (extend_heap(CHUNKSIZE / DSIZE) == NULL) {
		return -1;
	}

	// #ifdef DEBUG
	// 	mm_checkheap(__FUNCTION__);
	// #endif
		return 0;
}

static void *extend_heap(size_t dword)
{
	char *bp;
	size_t size;

	/* at least 4 WSIZE*/
	size = (dword % 2) ? (dword + 1) * DSIZE : dword * DSIZE;
	if ((long) (bp=mem_sbrk(size)) == (void *)-1) {
		return NULL;
	}

	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size, 0));
	PUT(NEXT_LINKNODE_RP(bp), 0);
	PUT(PREV_LINKNODE_RP(bp), 0);
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

	return coalesce(bp);
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;						/* adjusted block size */
    size_t extendsize;
    char* bp;

    if (size == 0) {
    	return NULL;
    }

    if (size <= DSIZE) {
    	asize = 2 * DSIZE;
    }
    else {
    	asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }

    /* search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
    	place(bp, asize);
    	// #ifdef DEBUG
    	// 	mm_checkheap(__FUNCTION__);
    	// #endif
    	return bp;
    }

    /* No fit found, get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / DSIZE)) == NULL) {
    	return NULL;
    }

    place(bp, asize);

 //    #ifdef DEBUG
	// 	mm_checkheap(__FUNCTION__);
	// #endif
	return bp;
}

/*
 * mm_free - update prev and next pointers.
 */
void mm_free(void *bp)
{
	if (bp == 0) return;
	size_t size = GET_SIZE(HDRP(bp));

	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size, 0));
	PUT(NEXT_LINKNODE_RP(bp), 0);
	PUT(PREV_LINKNODE_RP(bp), 0);
	coalesce(bp);

	// #ifdef DEBUG
	// 	mm_checkheap(__FUNCTION__);
	// #endif
}

static void *coalesce(void *bp)
{
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));

	if (prev_alloc && next_alloc) {
		;
	}
	else if (prev_alloc && !next_alloc) {
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));

		update_linkedlist(NEXT_BLKP(bp));

		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
	}
	else if (!prev_alloc && next_alloc) {
		size += GET_SIZE(FTRP(PREV_BLKP(bp)));

		update_linkedlist(PREV_BLKP(bp));

		PUT(FTRP(bp), PACK(size, 0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
	}
	else {
		size += GET_SIZE(FTRP(PREV_BLKP(bp)));
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));

		update_linkedlist(PREV_BLKP(bp));
		update_linkedlist(NEXT_BLKP(bp));
		
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
	}

	insert_linkedlist(bp);
    return bp;
}

/* LIFO insert, update prev and next pointer of prev and next block */
static inline void update_linkedlist(char* p)
{
    char *prevp = GET(PREV_LINKNODE_RP(p));
    char *nextp = GET(NEXT_LINKNODE_RP(p));
    if(prevp == NULL) {
        if(nextp != NULL)
            PUT(PREV_LINKNODE_RP(nextp),0);
        PUT(root,nextp);
    }
    else {
        if(nextp != NULL)
            PUT(PREV_LINKNODE_RP(nextp),prevp);
        PUT(NEXT_LINKNODE_RP(prevp),nextp);
    }
    PUT(NEXT_LINKNODE_RP(p),0);
    PUT(PREV_LINKNODE_RP(p),0);
}

/* insert free block bp into the root of the free block list */
inline void insert_linkedlist(char* p)
{
	/*p will be insert into the linklist ,LIFO*/
    char *nextp = GET(root);
    if(nextp != NULL)
        PUT(PREV_LINKNODE_RP(nextp),p);

    PUT(NEXT_LINKNODE_RP(p),nextp);
    PUT(root,p);
}

static void *find_fit(size_t size)
{
	int count = 0;
	int free_size, minDiff=0x7fffffff;
	char *tmp = NULL;
	/*first fit*/
    char *ptr = GET(root);
    while(ptr != NULL) {
    	free_size = GET_SIZE(HDRP(ptr));

        if (free_size >= size) {
        	int diff = free_size - size;
        	if (diff < minDiff) {
        		minDiff = diff;
        		tmp = ptr;
        		count++;
        	}
        	if (count >= 1)
        		return ptr;
        }

        ptr = GET(NEXT_LINKNODE_RP(ptr));
    } 
    return tmp;
}
	



static void place(void *bp, size_t asize)
{
	size_t csize = GET_SIZE(HDRP(bp));
	update_linkedlist(bp);

	if ((csize - asize) >= (2 * DSIZE)) {
		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));

		bp = NEXT_BLKP(bp);

		PUT(HDRP(bp), PACK(csize - asize, 0));
		PUT(FTRP(bp), PACK(csize - asize, 0));

		PUT(NEXT_LINKNODE_RP(bp), 0);
		PUT(PREV_LINKNODE_RP(bp), 0);

		coalesce(bp);
	} else {
		PUT(HDRP(bp), PACK(csize, 1));
		PUT(FTRP(bp), PACK(csize, 1));
	}
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *newptr;
    size_t oldSize;

    if (size == 0) {
    	mm_free(ptr);
    	return NULL;
    }

    if (ptr == NULL) {
    	return mm_malloc(size);
    }
    
    newptr = mm_malloc(size);
    if (!newptr)
    	return NULL;

    // oldSize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);

    oldSize = GET_SIZE(HDRP(ptr));
    if (size < oldSize) {
    	oldSize = size;
    }

    memcpy(newptr, ptr, oldSize);

    mm_free(ptr);

    return newptr;
}

int mm_checkheap(int verbose)
{
	
	
}

/*
static inline void checkblock(void *bp)
{
	if (!aligned(bp)) {
		printf("Error, %p dosen't satisfy wsize alignment \n", bp);
	}
}
*/













