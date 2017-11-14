#include "cachelab.h"
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#define ADDRESS_SIZE 64

typedef struct {
    char valid_bit;
    unsigned long tag;
    int LRU_count;
} Cache_line;

typedef struct {
    Cache_line* lines;
} Cache_set;

typedef struct {
    int S;
    int E;
    Cache_set* sets;
} Cache;

void init_cache(int s, int E, int b, Cache* cache);
void cacheSimulator(int s, int E, int b, char* file, int isVerbose, Cache* pCache);
int get_hitIndex(Cache *cache, int set_index, int tag);
int get_emptyIndex(Cache *cache, int set_index, int tag);

void store(Cache *cache, int set_index, int tag, int isVerbose);
void load(Cache *cache, int set_index, int tag, int isVerbose);
void modify(Cache *cache, int set_index, int tag, int isVerbose);

int hit_count;
int miss_count;
int eviction_count;

int main(int argc, char *argv[]) {
    int s, E, b;
    char file[100];
    int isVerbose = 0;
    Cache cache;
    hit_count = miss_count = eviction_count = 0;

    int ch;
    while ((ch = getopt(argc, argv, "vs:E:b:t:")) != -1) {
        switch (ch) {
            case 'v':
                isVerbose = 1;
                break;
            case 's':
                s = atoi(optarg);
                // printf("The argument of -s is %d\n\n", s);
                break;
            case 'E':
                E = atoi(optarg);
                // printf("The argument of -E is %d\n\n", E);
                break;
            case 'b':
                b = atoi(optarg);
                // printf("The argument of -b is %d\n\n", b);
                break;
            case 't':
                strcpy(file, optarg);
                break;
            default:
                // printf("Unknown option: %c\n", (char)optopt);
                break;
        }
    }

    init_cache(s, E, b, &cache);
    // printf("cache initialize done\n");

    cacheSimulator(s, E, b, file, isVerbose, &cache);

    printSummary(hit_count, miss_count, eviction_count);

    return 0;
}


void init_cache(int s, int E, int b, Cache* cache) {
    cache->S = 2 << s;
    cache->E = E;
    cache->sets = (Cache_set*) malloc(cache->S * sizeof(Cache_set));

    int i, j;
    for (i = 0; i < cache->S; i++) {
        cache->sets[i].lines = (Cache_line*) malloc(E * sizeof(Cache_line));
        for (j = 0; j < cache->E; j++) {
            cache->sets[i].lines[j].valid_bit = 0;
            cache->sets[i].lines[j].LRU_count = 0;
        }
    }

    return;
}

/* find empty line sequentially */
int get_emptyIndex(Cache *cache, int set_index, int tag) {
    int i;
    int emptyIndex = -1;
    for (i = 0; i < cache->E; ++i) {
        if (cache->sets[set_index].lines[i].valid_bit == 0) {
            emptyIndex = i;
            break;
        }
    }

    // if (emptyIndex != -1) {
    //     cache->sets[set_index].lines[emptyIndex].valid_bit = 1;
    //     cache->sets[set_index].lines[emptyIndex].tag = tag;
    // }

    return emptyIndex;
}


/**
    hit or miss
    return i if hit the ith line, -1 otherwise
 */
int get_hitIndex(Cache *cache, int set_index, int tag) {
    int i;
    int hitIndex = -1;

    for (i = 0; i < cache->E; i++) {
        if (cache->sets[set_index].lines[i].valid_bit == 1 && 
            cache->sets[set_index].lines[i].tag == tag) {
            hitIndex = i;
            break;
        }
    }

    return hitIndex;
}

void load(Cache *cache, int set_index, int tag, int isVerbose) {
    // hit or miss
    int hitIndex = get_hitIndex(cache, set_index, tag);
    if (hitIndex == -1) {               /* miss the cache */
        miss_count++;
        if (isVerbose) {
            printf("miss ");
        }
        /* eviction or insert sequentially */
        int emptyIndex = get_emptyIndex(cache, set_index, tag);     
        if (emptyIndex == -1) {         /* full, eviction */
            eviction_count++;
            if (isVerbose) {
                printf("eviction ");
            }

            int i;
            int firstFlag = 1;
            for (i = 0; i < cache->E; i++) {
                if (cache->sets[set_index].lines[i].LRU_count == cache->E - 1 && firstFlag) {
                    cache->sets[set_index].lines[i].valid_bit = 1;
                    cache->sets[set_index].lines[i].LRU_count = 0;
                    cache->sets[set_index].lines[i].tag = tag;
                    firstFlag = 0;
                } else {
                    cache->sets[set_index].lines[i].LRU_count++;
                }
            }
        } else {                        /* exists empty line, insert */
            int i;
            for (i = 0; i < cache->E; i++) {
                if (i != emptyIndex) {
                    cache->sets[set_index].lines[i].LRU_count++;
                } else {
                    cache->sets[set_index].lines[i].valid_bit = 1;
                    cache->sets[set_index].lines[i].tag = tag;
                    cache->sets[set_index].lines[i].LRU_count = 0;
                }
            }
        }
    } else {                            /* hit the cache */
        hit_count++;
        if (isVerbose) {
            printf("hit ");
        }

        int hit_count = cache->sets[set_index].lines[hitIndex].LRU_count;
        int i;
        for (i = 0; i < cache->E; i++) {
            if (i != hitIndex) {
                if (cache->sets[set_index].lines[i].LRU_count < hit_count) {
                    cache->sets[set_index].lines[i].LRU_count++;
                }
            } else {
                cache->sets[set_index].lines[i].LRU_count = 0;
            }
        }
    }
}

void store(Cache *cache, int set_index, int tag, int isVerbose) {
    load(cache, set_index, tag, isVerbose);
}

void modify(Cache *cache, int set_index, int tag, int isVerbose) {
    load(cache, set_index, tag, isVerbose);
    store(cache, set_index, tag, isVerbose);
}


void cacheSimulator(int s, int E, int b, char* file, int isVerbose, Cache* pCache) {
    FILE *pFile;                        /* pointer to FILE object */
    pFile = fopen(file, "r");
    char access_type;                   /* L-load S-store M-modify */
    unsigned long address;              /* 64-bit hexa memory address */
    int size;                           /* # of bytes accessed by operation */

    int tag_move_bits = b + s;

    while (fscanf(pFile, " %c %lx,%d", &access_type, &address, &size) > 0) {
        if (access_type == 'I') {
            continue;
        } else {
            int tag = address >> tag_move_bits;
            int set_index = (address >> b) & ((1 << s) - 1);
            /*
            printf("------------------------\n");
            printf("tag = %d\t\t", tag);
            printf("set_index = %d\n", set_index);
            */

            if (isVerbose == 1) {
                printf("%c %lx,%d ", access_type, address, size);
            }
            if (access_type == 'S') {
                store(pCache, set_index, tag, isVerbose);
            }
            if (access_type == 'M') {
                modify(pCache, set_index, tag, isVerbose);
            }
            if (access_type == 'L') {
                load(pCache, set_index, tag, isVerbose);
            }
            if (isVerbose == 1) {
                printf("\n");
            }
        }
    }

    fclose(pFile);
    return;
}