/**
 * @file mount.c
 * @brief accessing the UNIX v6 filesystem -- core of the first set of assignments
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "mount.h"
#include "unixv6fs.h"
#include "error.h"
#include "sector.h"
#include "inode.h"
#include <math.h>

/**
 * @brief   fill the bmblock array ibm of the struct unix_filesystem u
 * @param u the filesystem we want to fill its ibm (IN)
 */
void fill_ibm(struct unix_filesystem *u)
{
    if(u!=NULL) {
        struct inode tab[INODES_PER_SECTOR];
        for(int i=u->s.s_inode_start; i<u->s.s_inode_start+u->s.s_isize-1; ++i) {
            if(sector_read(u->f,i,tab)!=0) {
                for(int j=0; j<INODES_PER_SECTOR; ++j) {
                    bm_set(u->ibm,(i-u->s.s_inode_start)*INODES_PER_SECTOR+j);
                }
            }
            for(int j=0; j<INODES_PER_SECTOR; ++j) {
                if(tab[j].i_mode & IALLOC) {
                    bm_set(u->ibm,(i-u->s.s_inode_start)*INODES_PER_SECTOR+j);
                }
            }
        }
    }
}
/**
 * @brief  fill the bmblock array fbm of the struct unix_filesystem u
 * @param u the filesystem we want to fill its ibm (IN)
 */
void fill_fbm(struct unix_filesystem* u)
{
    if ((u!=NULL) && (u->ibm!=NULL)) {
        //i parcours toutes les inodes de ibm
        for(int i=u->ibm->min-1; i<u->ibm->max; ++i) {
            //si l'inode est allouée
            if((bm_get(u->ibm,i)==1)||(i==ROOT_INUMBER)) {
                struct inode ind;
                memset(&ind,0,sizeof(ind));
                if(inode_read(u,i,&ind)>=0) {
                    int index=0;
                    int32_t file_sec_off=0;
                    if(inode_getsize(&ind)>(ADDR_SMALL_LENGTH)*SECTOR_SIZE) {
						for(int m=0;m<8;++m)
						bm_set(u->fbm,ind.i_addr[m]);
					}
					int offset=0;
                    //tant qu'on a pas parcouru tout les secteurs correspondant à cette inode
                    while(offset<inode_getsize(&ind)) {
                        //on recupere le numero du prochain secteur où l'inode est stockée
                        if((index=inode_findsector(u,&ind,file_sec_off))>0) {
                            //on le met à 1 dans fbm
                            bm_set(u->fbm,index);
                            //on incremente le offset de SECTOR_SIZE a chaque fois
                            offset+=SECTOR_SIZE;
                            ++file_sec_off;
                        }
                    }
                }
            }
        }
    }
}

/**
 * @brief  mount a unix v6 filesystem
 * @param filename name of the unixv6 filesystem on the underlying disk (IN)
 * @param u the filesystem (OUT)
 * @return 0 on success; <0 on error
 */
int mountv6(const char *filename, struct unix_filesystem *u)
{

    uint8_t bootSector[SECTOR_SIZE];
    int r=1;

    M_REQUIRE_NON_NULL(filename);
    M_REQUIRE_NON_NULL(u);
    memset(u, 0, sizeof(*u));
    u->fbm = NULL;
    u->ibm = NULL;

    FILE* entree = fopen(filename,"r+b");
    if(entree==NULL) return ERR_IO;
    u->f=entree;

    if( (r=sector_read(u->f, BOOTBLOCK_SECTOR, bootSector)) != 0 ) return r;
    if(bootSector[BOOTBLOCK_MAGIC_NUM_OFFSET]!=BOOTBLOCK_MAGIC_NUM) return ERR_BADBOOTSECTOR;

    if( (r=sector_read(u->f, SUPERBLOCK_SECTOR, &(u->s))) != 0 ) return r;
    
    u->fbm = bm_alloc(u->s.s_block_start+1,u->s.s_fsize-1);
    if(u->fbm==NULL) return ERR_NOMEM;
    u->ibm = bm_alloc(u->s.s_inode_start,u->s.s_isize*INODES_PER_SECTOR-1);
    if(u->ibm==NULL) return ERR_NOMEM;
    fill_ibm(u);
    fill_fbm(u);

    return 0;

}

/**
 * @brief print to stdout the content of the superblock
 * @param u - the mounted filesytem
 */
void mountv6_print_superblock(const struct unix_filesystem *u)
{
    printf("**********FS SUPERBLOCK START**********\n");
    printf("s_isize             : %" PRIu16 "\n", u->s.s_isize);
    printf("s_fsize             : %" PRIu16 "\n", u->s.s_fsize);
    printf("s_fbmsize           : %" PRIu16 "\n", u->s.s_fbmsize);
    printf("s_ibmsize           : %" PRIu16 "\n", u->s.s_ibmsize);
    printf("s_inode_start       : %" PRIu16 "\n", u->s.s_inode_start);
    printf("s_block_start       : %" PRIu16 "\n", u->s.s_block_start);
    printf("s_fbm_start         : %" PRIu16 "\n", u->s.s_fbm_start);
    printf("s_ibm_start         : %" PRIu16 "\n", u->s.s_ibm_start);
    printf("s_flock             : %" PRIu8 "\n", u->s.s_flock);
    printf("s_ilock             : %" PRIu8 "\n", u->s.s_ilock);
    printf("s_fmod              : %" PRIu8 "\n", u->s.s_fmod);
    printf("s_ronly             : %" PRIu8 "\n", u->s.s_ronly);
    printf("s_time              : [0] %" PRIu16 "\n", u->s.s_time[0]); //voir pour print (tableau)
    printf("**********FS SUPERBLOCK END**********\n");
}

/**
 * @brief umount the given filesystem
 * @param u - the mounted filesytem
 * @return 0 on success; <0 on error
 */
int umountv6(struct unix_filesystem *u)
{
    M_REQUIRE_NON_NULL(u);
    int check=fclose(u->f);
    if(check!=0) {
        return ERR_IO;
    }
    return 0;
}

/**
 * @brief create a new filesystem
 * @param num_blocks the total number of blocks (= max size of disk), in sectors
 * @param num_inodes the total number of inodes
 */
int mountv6_mkfs(const char *filename, uint16_t num_blocks, uint16_t num_inodes){
	
	struct superblock sblock;
	memset(&sblock,0,sizeof(sblock));
	sblock.s_isize = ceil((double)num_inodes/INODES_PER_SECTOR);
	sblock.s_fsize = num_blocks;
	if(sblock.s_fsize<(sblock.s_isize+num_inodes)) return ERR_NOT_ENOUGH_BLOCS;
	sblock.s_inode_start = SUPERBLOCK_SECTOR+1;
	sblock.s_block_start = sblock.s_inode_start + sblock.s_isize;
	
	FILE* entree = fopen(filename,"w+b");
    if(entree==NULL) return ERR_IO;
	uint8_t bootSector[SECTOR_SIZE];
	bootSector[BOOTBLOCK_MAGIC_NUM_OFFSET] = BOOTBLOCK_MAGIC_NUM;
	int err=0;
	if( (err=sector_write(entree, BOOTBLOCK_SECTOR, bootSector)) != 0 ) return err;
	if( (err=sector_write(entree, SUPERBLOCK_SECTOR, &sblock)) != 0 ) return err;
	
	struct inode ino;
	memset(&ino,0,sizeof(ino));
	ino.i_mode = (uint16_t)IFDIR + IALLOC;
	struct inode inodes[INODES_PER_SECTOR];	
	for(int j=0;j<INODES_PER_SECTOR;++j){
		memset(&inodes[j], 0, sizeof(struct inode));
	}
	inodes[1]=ino;
	if( (err=sector_write(entree, sblock.s_inode_start, inodes)) != 0 ) return err;
	for(int j=0;j<INODES_PER_SECTOR;++j){
		memset(&inodes[j], 0, sizeof(struct inode));
	}
	for(int i=sblock.s_inode_start+1; i<sblock.s_block_start;++i){
		if( (err=sector_write(entree, i, inodes)) != 0 ) return err;	
	}
	fclose(entree);
	
	return 0;
}
