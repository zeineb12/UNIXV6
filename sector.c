/**
 * @file  sector.c
 * @brief block-level accessor function.
 */

#include <stdio.h>
#include "error.h"
#include "sector.h"
#include "unixv6fs.h"

/**
 * @brief read one 512-byte sector from the virtual disk
 * @param f open file of the virtual disk
 * @param sector the location (in sector units, not bytes) within the virtual disk
 * @param data a pointer to 512-bytes of memory (OUT)
 * @return 0 on success; <0 on error
 */
int sector_read(FILE *f, uint32_t sector, void *data)
{
    size_t nbr_bytes_read =0;
    int pos_sector = sector * SECTOR_SIZE; //position in bytes of the sector on the disk

    M_REQUIRE_NON_NULL(f);
    M_REQUIRE_NON_NULL(data);
    if(fseek(f,pos_sector,SEEK_SET)!=0) {
        return ERR_IO;
    }
    nbr_bytes_read=fread(data,1,SECTOR_SIZE,f);
    if(nbr_bytes_read<SECTOR_SIZE) {
        return ERR_IO;
    }
    return 0; //in case of success
}

/**
 * @brief writes one 512-byte sector from the virtual disk
 * @param f open file of the virtual disk
 * @param sector the location (in sector units, not bytes) within the virtual disk
 * @param data a pointer to 512-bytes of memory (IN)
 * @return 0 on success; <0 on error
 */
int sector_write(FILE *f, uint32_t sector, void  *data)
{
	M_REQUIRE_NON_NULL(f);
    M_REQUIRE_NON_NULL(data);
    size_t nbr_bytes_written =0;
    int pos_sector = sector * SECTOR_SIZE; //position in bytes of the sector on the disk
	if(fseek(f,pos_sector,SEEK_SET)!=0) {
        return ERR_IO;
    }
    nbr_bytes_written = fwrite(data,1,SECTOR_SIZE,f);
    if(nbr_bytes_written!=SECTOR_SIZE) {
        return ERR_IO;
    }
    return 0;
}
