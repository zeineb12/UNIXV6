/**
 * @file bmblock.c
 * @brief utility functions for the UNIX v6 filesystem.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "error.h"
#include "bmblock.h"

/**
 * @brief allocate a new bmblock_array to handle elements indexed
 * between min and may (included, thus (max-min+1) elements).
 * @param min the mininum value supported by our bmblock_array
 * @param max the maxinum value supported by our bmblock_array
 * @return a pointer of the newly created bmblock_array or NULL on failure
 */
struct bmblock_array *bm_alloc(uint64_t min, uint64_t max)
{
    if(max<min) return NULL;
    size_t length = (size_t) ceil((max-min+1)/(double)BITS_PER_VECTOR);
    struct bmblock_array *bmblock = calloc(length*sizeof(uint64_t)+sizeof(struct bmblock_array),sizeof(uint8_t));
    if(NULL==bmblock) return NULL;
    bmblock->length=length;
    bmblock->cursor=0;
    bmblock->min=min;
    bmblock->max=max;
    return bmblock;
}

/**
 * @brief free the memory associated to the bmblock pointer
 * @param bmblock the pointer containing the struct bmblock_array
 */
void bm_free(struct bmblock_array *bmblock)
{
    free(bmblock);
    bmblock=NULL;
}

/**
 * @brief return the bit associated to the given value
 * @param bmblock_array the array containing the value we want to read
 * @param x an integer corresponding to the number of the value we are looking for
 * @return <0 on failure, 0 or 1 on success
 */
int bm_get(struct bmblock_array *bmblock_array, uint64_t x)
{
    M_REQUIRE_NON_NULL(bmblock_array);
    if ((x>bmblock_array->max)||(x<bmblock_array->min)) return ERR_BAD_PARAMETER;
    size_t index = (size_t)(floor((x-bmblock_array->min)/(double)BITS_PER_VECTOR));
    /*le minimum correspondant à la bonne case du tableau*/
    size_t min_index= bmblock_array->min + (BITS_PER_VECTOR*index);
    uint64_t mask= UINT64_C(1)<<(x-min_index);
    mask &= bmblock_array->bm[index];
    return (int) (mask>>(x-min_index));
}

/**
 * @brief set to true (or 1) the bit associated to the given value
 * @param bmblock_array the array containing the value we want to set
 * @param x an integer corresponding to the number of the value we are looking for
 */
void bm_set(struct bmblock_array *bmblock_array, uint64_t x)
{
    if(bmblock_array!=NULL) {
        if ((x<=bmblock_array->max)&&(x>=bmblock_array->min)) {
            size_t index = (size_t)floor((x-bmblock_array->min)/(double)BITS_PER_VECTOR);
            /*le minimum correspondant à la bonne case du tableau(index)*/
            size_t min_index= bmblock_array->min + (BITS_PER_VECTOR*index);
            uint64_t mask= UINT64_C(1)<<(x-min_index);
            bmblock_array->bm[index] |= mask;
        }
    }
}

/**
 * @brief set to false (or 0) the bit associated to the given value
 * @param bmblock_array the array containing the value we want to clear
 * @param x an integer corresponding to the number of the value we are looking for
 */
void bm_clear(struct bmblock_array *bmblock_array, uint64_t x)
{
    if(bmblock_array!=NULL) {
        if ((x<=bmblock_array->max)&&(x>=bmblock_array->min)) {
            size_t index = (size_t)floor((x-bmblock_array->min)/(double)BITS_PER_VECTOR);
            /*le minimum correspondant à la bonne case du tableau(index)*/
            size_t min_index= bmblock_array->min + (BITS_PER_VECTOR*index);
            uint64_t mask= ( ~(UINT64_C(0)) ) ^ ( UINT64_C(1)<<(x-min_index) );
            bmblock_array->bm[index] &= mask;
            if(bmblock_array->cursor>index) bmblock_array->cursor=index;
        }
    }
}

/**
 * @brief return the next unused bit
 * @param bmblock_array the array we want to search for place
 * @return <0 on failure, the value of the next unused value otherwise
 */
int bm_find_next(struct bmblock_array *bmblock_array)
{
    //tant que le curseur n'est pas à la fin
    while((bmblock_array->cursor<bmblock_array->length)) {
        //si le curseur "pointe sur une case qui n'est pas toute remplie"
        if (bmblock_array->bm[bmblock_array->cursor] != UINT64_C(-1)) {
            // au moins un bit de n est nul, le chercher
            for(int i=0; i<BITS_PER_VECTOR; ++i) {
                if(bm_get(bmblock_array,bmblock_array->min+bmblock_array->cursor*BITS_PER_VECTOR+i)==0) {
                    return bmblock_array->min+bmblock_array->cursor*BITS_PER_VECTOR+i;
                }
            }
        } else {
            ++(bmblock_array->cursor);
        }
    }
    return ERR_BITMAP_FULL;
}
/**
 * @brief auxiliar method used in bm_print to print an uint64_t in the right order
 * @param uint64_t u the unsigned int we want to print
 */
void ordered_uint64_print(uint64_t u)
{
    uint64_t mask = 1;
    uint64_t result = u;
    for(int j=0; j<sizeof(uint64_t); ++j) {
        for(int k=0; k<8; ++k) {
            printf("%d",(int)(result&mask));
            result=result>>1;
        }
        printf(" ");
    }
    printf("\n");
}

/**
 * @brief usefull to see (and debug) content of a bmblock_array
 * @param bmblock_array the array we want to see
 */
void bm_print(struct bmblock_array *bmblock_array)
{
    if(bmblock_array!=NULL) {
        puts("**********BitMap Block START**********");
        printf("length: %lu\n",bmblock_array->length);
        printf("min: %lu\n",bmblock_array->min);
        printf("max: %lu\n",bmblock_array->max);
        printf("cursor: %lu\n",bmblock_array->cursor);
        puts("content: ");
        for(int i=0; i<bmblock_array->length; ++i) {
            printf("%d:  ",i);
            ordered_uint64_print(bmblock_array->bm[i]);
        }
        puts("**********BitMap Block END************");
    }
}

