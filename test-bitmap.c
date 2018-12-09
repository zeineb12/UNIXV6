#include <stdio.h>
#include "bmblock.h"

int main (void){
	struct bmblock_array *bmblock =bm_alloc(UINT64_C(4),UINT64_C(131));
	bm_print(bmblock);
	printf("find_next() = %d\n",bm_find_next(bmblock));
	bm_set(bmblock,4);
	bm_set(bmblock,5);
	bm_set(bmblock,6);
	bm_print(bmblock);
	printf("find_next() = %d\n",bm_find_next(bmblock));
	for(int i=4;i<131;i+=3){
		bm_set(bmblock,i);
	}
	bm_print(bmblock);
	printf("find_next() = %d\n",bm_find_next(bmblock));
	for(int i=5;i<131;i+=5){
		bm_clear(bmblock,i);
	}
	bm_print(bmblock);
	printf("find_next() = %d\n",bm_find_next(bmblock));
	bm_free(bmblock);
	return 0;
}
