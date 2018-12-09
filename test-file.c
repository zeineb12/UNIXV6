/**
 * @file test-file.c
 * @brief tests the filev6.c functions
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "unixv6fs.h"
#include "inode.h"
#include "error.h"
#include "filev6.h"
#include "sha.h"

void print_inode(struct unix_filesystem *u, uint16_t inr){
	struct filev6 fs;
    uint8_t secteur[SECTOR_SIZE+1];
    secteur[SECTOR_SIZE] = '\0';
    int r1 = filev6_open(u,inr,&fs);
    if(r1!=0) {
        printf("filev6_open failed for inode #%d \n",inr);
    } else {
        printf("\nprinting inode #%"PRIu16":\n",inr);
        inode_print(&(fs.i_node));
        if(fs.i_node.i_mode & IALLOC) {
            if(fs.i_node.i_mode & IFDIR) {
                printf("Which is a directory\n");
            } else {
                filev6_readblock(&fs,secteur);
                printf("the first sector of data of which contains:\n%s\n\n----\n",secteur);
            }
        }
    }

}



int test(struct unix_filesystem *u)
{
    print_inode(u,(uint16_t)3);
    print_inode(u,(uint16_t)5);

	struct filev6 fs;
    printf("\nListing inodes SHA:\n");
    for(int i=0; i<(u->s.s_isize*INODES_PER_SECTOR); i++) {
        filev6_open(u,i,&fs);
        print_sha_inode(u,fs.i_node,i);
    }


    return 0;
}
