/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, ii, jj, tmp, tmp1, tmp2, tmp3;
    int bsize = 8;

    // s=5 -> S=32 sets b=5 -> B=32B=8 int
    // 32*32按照8*8分组块，一行4块，正好可以填满cache
    // 这样既充分利用了cache行的8个字节的block也充分利用了工作集
    if (M == 32) {                                  /* block multiply, fit memory to solve conflict miss */
        for (ii = 0; ii < N; ii += bsize) {
            for (jj = 0; jj < M; jj += bsize) {
                for (i = ii; i < ii + bsize; i++) {
                    for (j = jj; j < jj + bsize; j++) {
                        if (i != j) {
                            B[j][i] = A[i][j];
                        } else {
                            tmp = A[i][j];
                        }
                    }
                    if (ii == jj) {                 /* decrease eviction along the diagonal */
                        B[i][i] = tmp;
                    }
                } 
            }
        }
    } else if (M == 64) {                           /* choose proper bsize */
        for (ii = 0; ii < N; ii += bsize) {
            for (jj = 0; jj < M; jj += bsize) {
                // 左上角
                j = jj;
                for (i = ii; i < ii + 4; i++) {     /* 这里引入4个局部变量来存储4*4小块的值 */
                    tmp = A[i][j];
                    tmp1 = A[i][j + 1];
                    tmp2 = A[i][j + 2];
                    tmp3 = A[i][j + 3];
                    B[j][i] = tmp;
                    B[j + 1][i] = tmp1;
                    B[j + 2][i] = tmp2;
                    B[j + 3][i] = tmp3;
                }
                // 右上角
                j = jj + 4;
                for (i = ii; i < ii + 4; i++) {
                    tmp = A[i][j];
                    tmp1 = A[i][j + 1];
                    tmp2 = A[i][j + 2];
                    tmp3 = A[i][j + 3];
                    B[j][i] = tmp;
                    B[j + 1][i] = tmp1;
                    B[j + 2][i] = tmp2;
                    B[j + 3][i] = tmp3;
                }
                // 右下角
                for (i = ii + 4; i < ii + 8; i++) {
                    tmp = A[i][j];
                    tmp1 = A[i][j + 1];
                    tmp2 = A[i][j + 2];
                    tmp3 = A[i][j + 3];
                    B[j][i] = tmp;
                    B[j + 1][i] = tmp1;
                    B[j + 2][i] = tmp2;
                    B[j + 3][i] = tmp3;
                }
                // 左下角放在最后也是为了减少冲突miss
                j = jj;
                for (i = ii + 4; i < ii + 8; i++) {
                    tmp = A[i][j];
                    tmp1 = A[i][j + 1];
                    tmp2 = A[i][j + 2];
                    tmp3 = A[i][j + 3];
                    B[j][i] = tmp;
                    B[j + 1][i] = tmp1;
                    B[j + 2][i] = tmp2;
                    B[j + 3][i] = tmp3;
                }
            }
        }
    } else {                                        /* 这里选择合适的块大小，满足miss的要求即可 */
        bsize = 16;
        for (ii = 0; ii < N; ii += bsize) {
            for (jj = 0; jj < M; jj += bsize) {
                j = 0;
                for (i = ii; (i < ii + bsize) && (i < N); i++) {
                    for (j = jj; (j < jj + bsize) && (j < M); j++) {
                        if (i != j) {
                            B[j][i] = A[i][j];
                        } else {
                            tmp = A[i][j];
                        }
                    }
                    if (ii == jj) {
                        B[i][i] = tmp;
                    }
                } 
            }
        }
        
    }
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

