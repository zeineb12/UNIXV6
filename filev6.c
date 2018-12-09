/**
 * @file filev6.c
 * @brief accessing the UNIX v6 filesystem -- file part of inode/file layer
 *
 */
#include <string.h>
#include "mount.h"
#include "filev6.h"
#include "error.h"
#include "inode.h"
#include "unixv6fs.h"
#include <math.h>

#define MAX_SIZE_FILE 7*256*512
#define MAX_SMALL_FILE 4*1000

/**
 * @brief open up a file corresponding to a given inode; set offset to zero
 * @param u the filesystem (IN)
 * @param inr he inode number (IN)
 * @param fv6 the complete filve6 data structure (OUT)
 * @return 0 on success; <0 on errror
 */
int filev6_open(const struct unix_filesystem *u, uint16_t inr, struct filev6 *fv6)
{
    int r=1;
    M_REQUIRE_NON_NULL(u);
    M_REQUIRE_NON_NULL(fv6);
    memset(fv6,0,sizeof(*fv6));
    fv6->u=u;
    fv6->i_number=inr;
    fv6->offset=0;
    if ((r=inode_read(u, inr, &(fv6->i_node))) !=0) return r;
    return 0;
}

/**
 * @brief read at most SECTOR_SIZE from the file at the current cursor
 * @param fv6 the filev6 (IN-OUT; offset will be changed)
 * @param buf points to SECTOR_SIZE bytes of available memory (OUT)
 * @return >0: the number of bytes of the file read; 0: end of file; <0 error
 */
int filev6_readblock(struct filev6 *fv6, void *buf)
{
    int r=0;
    int sector_number=-1;
    int bytes_read=0;
    M_REQUIRE_NON_NULL(fv6);
    M_REQUIRE_NON_NULL(buf);

    //offset qui depasse la taille de l'inode
    if( (fv6->offset)>inode_getsize(&fv6->i_node)) return ERR_OFFSET_OUT_OF_RANGE;
    //offset égal à la taille en bytes de l'inode
    else if( (fv6->offset)==inode_getsize(&fv6->i_node)) return 0;

    else {
        bytes_read =inode_getsize(&(fv6->i_node))-fv6->offset;
        if( (sector_number=inode_findsector(fv6->u, &(fv6->i_node), (fv6->offset)/SECTOR_SIZE)) <0) return sector_number;

        //offset ne désigne pas le dernier secteur à lire de l'inode
        if(bytes_read > SECTOR_SIZE) {
            bytes_read=SECTOR_SIZE;
            if( (r=sector_read(fv6->u->f,sector_number,buf)) <0 ) return r;
        }
        //offset désigne le dernier secteur à lire de l'inode (le secteur n'a donc pas forcement 512 octets remplis)
        else {
            if( (r=sector_read(fv6->u->f,sector_number,buf)) <0 ) return r;
        }

        fv6->offset+= bytes_read;
        return bytes_read;
    }
}

/**
 * @brief change the current offset of the given file to the one specified
 * @param fv6 the filev6 (IN-OUT; offset will be changed)
 * @param off the new offset of the file
 * @return 0 on success; <0 on errror
 */
int filev6_lseek(struct filev6 *fv6, int32_t offset)
{
	if((offset>=inode_getsize(&fv6->i_node))||(offset<0)) return ERR_OFFSET_OUT_OF_RANGE;
	else {
		fv6->offset=offset;
	}
	
    return 0;
}

/**
 * @brief create a new filev6
 * @param u the filesystem (IN)
 * @param mode the new offset of the file
 * @param fv6 the filev6 (IN-OUT; offset will be changed)
 * @return 0 on success; <0 on errror
 */
int filev6_create(struct unix_filesystem *u, uint16_t mode, struct filev6 *fv6)
{
	struct inode ino;
	memset(&ino,0,sizeof(ino));
	ino.i_mode=mode;
	int err = 0;
	if((err = inode_write(u,fv6->i_number,&ino))<0) return err;
	memcpy(&fv6->i_node,&ino,sizeof(ino));
    return 0;
}

/**
 * @brief write the len bytes of the given buffer on disk to the given filev6
 * @param u the filesystem (IN)
 * @param fv6 the filev6 (IN)
 * @param buf the data we want to write (IN)
 * @param len the length of the bytes we want to write
 * @return 0 on success; <0 on errror
 */

int filev6_writesector(struct unix_filesystem *u, struct filev6 *fv6, void *buf, int len)
{
	M_REQUIRE_NON_NULL(u);
	M_REQUIRE_NON_NULL(fv6);
	M_REQUIRE_NON_NULL(buf);
	
	int err=0;
	/* actual size of the file */
	int inode_size = inode_getsize(&fv6->i_node);
	if(inode_size>MAX_SIZE_FILE) return ERR_FILE_TOO_LARGE;
	int nb_bytes = 0;
	int taille_rest1 = len-(fv6->offset);
	int index =0;
	
	/* The size is a multiple of sector size */
	if(inode_size%SECTOR_SIZE ==0){	
		/* We find the next sector */
		int num_sector = bm_find_next(u->fbm);
		if(num_sector<0) return num_sector;
		if(taille_rest1<=SECTOR_SIZE){
			nb_bytes = taille_rest1;
		}else{
			nb_bytes = SECTOR_SIZE;
		}
		/* We wrote all the data from buf */
		if(nb_bytes ==0) return 0;
		
		/* Else we write the new sector */
		if((err=sector_write(u->f, num_sector, buf))<0) return err;
		bm_set(u->fbm,num_sector);
		
		/* We update the foffset */
		fv6->offset += nb_bytes;
		int counter = inode_size + nb_bytes;
		index = floor(counter/SECTOR_SIZE);
		/* We update the adresses of the inode */
		fv6->i_node.i_addr[index]= num_sector;
		/* We update the size */ 
		int total_size= inode_size+nb_bytes;
		inode_setsize(&(fv6->i_node), total_size);		
	}else{
		/* How many bytes does the last sector od data contain */
		int rempli = inode_size%SECTOR_SIZE;
		int taille_rest2 = SECTOR_SIZE-rempli;
		if(taille_rest1 <= taille_rest2){
			nb_bytes = taille_rest1;
		}else{
			nb_bytes = taille_rest2;
		}
		/* We wrote all the data */
		if(nb_bytes == 0) return 0;
		/* Else we get the last sector and we read it */
		index = floor(inode_size/SECTOR_SIZE);
		uint8_t secteur[SECTOR_SIZE];
		if((err=sector_read(u->f, fv6->i_node.i_addr[index],secteur))<0) return err;
		/* Then we update it with the information from buf */
		memcpy(secteur+rempli,buf,nb_bytes);
		/* We write the updated sector */
		if((err=sector_write(u->f, fv6->i_node.i_addr[index],secteur))<0) return err; 
		/* We update the offset and the total size */
		fv6->offset += nb_bytes;
		int total_size= inode_size+nb_bytes;
		inode_setsize(&(fv6->i_node), total_size);
	}
	putchar('\n');
	printf("write sector is : %d\n",nb_bytes);
	
	return nb_bytes;
	
}

/**
 * @brief write the len bytes of the given buffer on disk to the given filev6
 * @param u the filesystem (IN)
 * @param fv6 the filev6 (IN)
 * @param buf the data we want to write (IN)
 * @param len the length of the bytes we want to write
 * @return 0 on success; <0 on errror
 */
int filev6_writebytes(struct unix_filesystem *u, struct filev6 *fv6, void *buf, int len)
{
	M_REQUIRE_NON_NULL(u);
	M_REQUIRE_NON_NULL(fv6);
	M_REQUIRE_NON_NULL(buf);
	
	int size = inode_getsize(&(fv6->i_node));
	
	if((size+len) > MAX_SMALL_FILE) return ERR_FILE_TOO_LARGE; 
	int written =0;
	int err =0;
	int taille = len;
	
	/* While we didn't write all the data */
	while((written= filev6_writesector(u,fv6,buf,len))>0){
		size_t i=0;
		/* We decrement the size of buf by how much we wrote */
		taille-=written;
		if(taille>0){
			/* We increment buf untill we get to the new data */
			while(i<written){
				++buf;
				++i;
			}
		}
	}
	/* If we got an error */
	if(written<0) return written;
	/* We write the new inode */
	if((err=inode_write(u,fv6->i_number,&(fv6->i_node)))<0) return err;
	
    return 0;
}



