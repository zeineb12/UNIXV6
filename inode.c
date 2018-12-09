/**
 * @file inode.c
 * @brief accessing the UNIX v6 filesystem -- core of the first set of assignments
 */
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "unixv6fs.h"
#include "inode.h"
#include "error.h"
#include "sector.h"
#include "bmblock.h"

/**
 * @brief read all inodes from disk and print out their content to
 *        stdout according to the assignment
 * @param u the filesystem
 * @return 0 on success; < 0 on error.
 */
int inode_scan_print(const struct unix_filesystem *u)
{
#define INODE_SIZE 32
    M_REQUIRE_NON_NULL(u);
    int valeur=0;
    int r=1;
    uint8_t secteurs[SECTOR_SIZE];
    int numinode=0;
    for(int m=0; m<(u->s.s_isize); ++m) {
        if((r=sector_read(u->f,(u->s.s_inode_start+m),secteurs))!=0) {
            return r;
        }
        for(int i=0; i<SECTOR_SIZE; i+=INODE_SIZE) {
            struct inode ino;
            memset(&ino, 0, sizeof(ino));
            ino.i_mode = ((uint16_t)secteurs[i+1])<<8 | (uint16_t)secteurs[i];
            ino.i_nlink = secteurs[i+2];
            ino.i_uid = secteurs[i+3];
            ino.i_gid = secteurs[i+4];
            ino.i_size0 =  secteurs[i+5];
            ino.i_size1 = ((uint16_t)secteurs[i+7])<<8 | (uint16_t)secteurs[i+6];
            int k = i+8;
            for(int j=0; j<ADDR_SMALL_LENGTH; ++j) {
                ino.i_addr[j] = ((uint16_t)secteurs[k+1])<<8 | (uint16_t)secteurs[k];
                k+=2;
            }
            uint16_t iatime[2] = {((uint16_t)secteurs[k+1])<<8 | (uint16_t)secteurs[k], ((uint16_t)secteurs[k+3])<<8 | (uint16_t)secteurs[k+2]};
            for(int l=0; l<2; ++l) {
                ino.i_atime[l]=iatime[l];
            }
            uint16_t imtime[2] = {((uint16_t)secteurs[k+5])<<8 | (uint16_t)secteurs[k+4], ((uint16_t)secteurs[k+7])<<8 | (uint16_t)secteurs[k+6]};
            for(int l=0; l<2; ++l) {
                ino.i_mtime[l]=imtime[l];
            }
            if(ino.i_mode & IALLOC) {
                if(ino.i_mode & IFDIR) {
                    printf("inode   %d  (%s) len   %d \n",numinode,SHORT_DIR_NAME,inode_getsize(&ino));
                } else {
                    printf("inode   %d  (%s) len   %d \n",numinode,SHORT_FIL_NAME,inode_getsize(&ino));
                }
            }
            ++numinode;
        }
    }
    return valeur;
}

/**
 * @brief prints the content of an inode structure
 * @param inode the inode structure to be displayed
 */
void inode_print(const struct inode *inode)
{
    printf("**********FS INODE START**********\n");
    if( inode ==NULL) {
        printf("NULL ptr \n");
    } else {
        printf("i_mode: %" PRIu16 "\n", inode->i_mode);
        printf("i_nlink: %" PRIu8 "\n", inode->i_nlink);
        printf("i_uid: %" PRIu8 "\n", inode->i_uid);
        printf("i_gid: %" PRIu8 "\n", inode->i_gid);
        printf("i_size0: %" PRIu8 "\n", inode->i_size0);
        printf("i_size1: %" PRIu16 "\n", inode->i_size1);
        printf("size: %d \n", inode_getsize(inode));
    }
    printf("**********FS INODE END**********\n");
}

/**
 * @brief read the content of an inode from disk
 * @param u the filesystem (IN)
 * @param inr the inode number of the inode to read (IN)
 * @param inode the inode structure, read from disk (OUT)
 * @return 0 on success; <0 on error
 */
int inode_read(const struct unix_filesystem *u, uint16_t inr, struct inode *inode)
{
    /* initialisations */
    int sector_nbr=0;
    int r=1;
    uint8_t my_sector[SECTOR_SIZE];
    int counter=0;
    memset(inode, 0, sizeof(*inode));

    /* propager les erreurs s'il y en a */
    M_REQUIRE_NON_NULL(u);
    M_REQUIRE_NON_NULL(inode);

    /* si un des secteurs de u n'est pas lisible */
    for(int i=0; i<(u->s.s_isize); ++i) {
        if((r=sector_read(u->f,(u->s.s_inode_start+i),my_sector))!=0) return r;
    }

    /* si le numéro d'inode est invalide */
    if ((inr<0)||(inr>(u->s.s_isize*INODES_PER_SECTOR-1))) {
        return ERR_INODE_OUTOF_RANGE;
    }


    /* il faut lire secteur par secteur jusqu'a arriver au secteur contenant le bon inode */
    while(counter<=inr) {
        sector_read(u->f,(u->s.s_inode_start+sector_nbr),my_sector);
        counter+=INODES_PER_SECTOR;
        ++sector_nbr;
    }
    /* affectation de tout les parametres */
    int i= (inr-((sector_nbr-1)*INODES_PER_SECTOR))*INODE_SIZE;
    inode->i_mode = ((uint16_t)my_sector[i+1])<<8 | (uint16_t)my_sector[i];
    inode->i_nlink = my_sector[i+2];
    inode->i_uid = my_sector[i+3];
    inode->i_gid = my_sector[i+4];
    inode->i_size0 =  my_sector[i+5];
    inode->i_size1 = ((uint16_t)my_sector[i+7])<<8 | (uint16_t)my_sector[i+6];
    int k = i+8;
    for(int j=0; j<ADDR_SMALL_LENGTH; ++j) {
        inode->i_addr[j] = ((uint16_t)my_sector[k+1])<<8 | (uint16_t)my_sector[k];
        k+=2;
    }
    uint16_t iatime[2] = {((uint16_t)my_sector[k+1])<<8 | (uint16_t)my_sector[k],
                          ((uint16_t)my_sector[k+3])<<8 | (uint16_t)my_sector[k+2]
                         };
    for(int l=0; l<2; ++l) {
        inode->i_atime[l]=iatime[l];
    }
    uint16_t imtime[2] = {((uint16_t)my_sector[k+5])<<8 | (uint16_t)my_sector[k+4],
                          ((uint16_t)my_sector[k+7])<<8 | (uint16_t)my_sector[k+6]
                         };
    for(int l=0; l<2; ++l) {
        inode->i_mtime[l]=imtime[l];
    }
    /* si l'inode n'est pas alloué erreur */
    if(((inode->i_mode) & IALLOC)==0) return ERR_UNALLOCATED_INODE;

    return 0;

}

/**
 * @brief identify the sector that corresponds to a given portion of a file
 * @param u the filesystem (IN)
 * @param inode the inode (IN)
 * @param file_sec_off the offset within the file (in sector-size units)
 * @return >0: the sector on disk;  <0 error
 */
int inode_findsector(const struct unix_filesystem *u, const struct inode *i, int32_t file_sec_off)
{
	#define MAX_SIZE 7*256
    M_REQUIRE_NON_NULL(u);
    M_REQUIRE_NON_NULL(i);
    /* taille du fichier en bytes car getsize renvoie la taille en byte*/
    int32_t size_file = inode_getsize(i);
    int r=1;
    if((file_sec_off)*SECTOR_SIZE +1 >size_file) {
        return ERR_OFFSET_OUT_OF_RANGE;
    }
    /* L'inode n'est pas allouée */
    if((i->i_mode & IALLOC)==0) {
        return ERR_UNALLOCATED_INODE;
    }
    if(size_file<=ADDR_SMALL_LENGTH*SECTOR_SIZE) {
        return i->i_addr[file_sec_off];
    }
    if((size_file>(ADDR_SMALL_LENGTH)*SECTOR_SIZE)&&(size_file<=MAX_SIZE*SECTOR_SIZE)) {
        uint16_t adresses[ADDRESSES_PER_SECTOR];
        int secteur_indirect = file_sec_off/ADDRESSES_PER_SECTOR;
        if((r = sector_read(u->f, i->i_addr[secteur_indirect], adresses))!=0) {
            return r;
        }
        /* On retourne le numero du secteur voulu contenu dans l'element d'indice offset mod 256*/
        return adresses[file_sec_off % ADDRESSES_PER_SECTOR];
    } else {
        return ERR_FILE_TOO_LARGE;
    }
}

/**
 * @brief set the size of a given inode to the given size
 * @param inode the inode
 * @param new_size the new size
 * @return 0 on success; <0 on error
 */
int inode_setsize(struct inode *inode, int new_size)
{
	if(new_size<0) return ERR_NOMEM;
	inode->i_size0 = (uint8_t)(new_size >> 16);
	inode->i_size1 = (uint16_t)((new_size <<8 )>>8); 
    return 0;
}

/**
 * @brief alloc a new inode (returns its inr if possible)
 * @param u the filesystem (IN)
 * @return the inode number of the new inode or error code on error
 */
int inode_alloc(struct unix_filesystem *u)
{
    M_REQUIRE_NON_NULL(u);
    int next = bm_find_next(u->ibm);
	if(next<0) return ERR_NOMEM;
	bm_set(u->ibm, next);
	return next;
}

/**
 * @brief write the content of an inode to disk
 * @param u the filesystem (IN)
 * @param inr the inode number of the inode to read (IN)
 * @param inode the inode structure, read from disk (IN)
 * @return 0 on success; <0 on error
 */
int inode_write(struct unix_filesystem *u, uint16_t inr, struct inode *inode)
{
	/* initialisations */
    int index=0;
    int r=1;
    struct inode inodes[INODES_PER_SECTOR];
    int counter=0;
	int err = 0;

    /* propager les erreurs s'il y en a */
    M_REQUIRE_NON_NULL(u);
    M_REQUIRE_NON_NULL(inode);

    /* si un des secteurs de u n'est pas lisible */
    for(int i=0; i<(u->s.s_isize); ++i) {
        if((r=sector_read(u->f,(u->s.s_inode_start+i),inodes))!=0) return r;
    }
	
	/* si le numéro d'inode est invalide */
    if ((inr<0)||(inr>(u->s.s_isize*INODES_PER_SECTOR-1))) {
        return ERR_INODE_OUTOF_RANGE;
    }
    
    /* il faut lire secteur par secteur jusqu'a arriver au secteur contenant le bon inode */
    while(counter<=inr) {
        sector_read(u->f,(u->s.s_inode_start+index),inodes);
        counter+=INODES_PER_SECTOR;
        ++index;
    }
    --index;
    int sector_nbr=u->s.s_inode_start+index;
    
    /* écriture du secteur contenant le nouvel inode */
    inodes[inr]=*inode;
    if((err = sector_write(u->f,sector_nbr,inodes))<0) return err;
    
    return 0;
}

