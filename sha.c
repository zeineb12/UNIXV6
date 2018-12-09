/**
 * @file sha.c
 * @brief generate and print sha codes for given inodes
 */

#include <stdio.h>
#include <string.h>
#include "inode.h"
#include "filev6.h"
#include <openssl/sha.h>

/**
 * @brief transforms the unsigned char pointer returned by the SHA256 method into a "string" of chars
 * @param SHA the unsigned char string we want to transform (IN)
 * @param sha_string a string of chars (OUT)
 */
static void sha_to_string(const unsigned char *SHA, char *sha_string)
{
    if ((SHA == NULL) || (sha_string == NULL)) {
        return;
    }
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        sprintf(&sha_string[i * 2], "%02x", SHA[i]);
    }
    sha_string[2 * SHA256_DIGEST_LENGTH] = '\0';
}

/**
 * @brief print the sha of the content
 * @param content the content of which we want to print the sha
 * @param length the length of the content
 */
void print_sha_from_content(const unsigned char *content, size_t length)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    char sha_string [2*SHA256_DIGEST_LENGTH];
    SHA256(content, length, hash);
    sha_to_string(hash,sha_string);
    if(sha_string == NULL) {
        printf("La variable d'entree est nulle! \n");
    } else {
        for(int i=0; i<2*SHA256_DIGEST_LENGTH; ++i) {
            printf("%c",sha_string[i]);
        }
    }
}

/**
 * @brief print the sha of the content of an inode
 * @param u the filesystem
 * @param inode the inocde of which we want to print the content
 * @param inr the inode number
 */
void print_sha_inode(struct unix_filesystem *u, struct inode inode, int inr)
{
    int32_t size_file = inode_getsize(&inode);
    uint8_t my_sector[SECTOR_SIZE];
    uint8_t val[size_file];
    int index=0;
    struct filev6 file;
    memset(&file, 0, sizeof(struct filev6));
    filev6_open(u,inr,&file);
    if(file.i_node.i_mode & IALLOC) {
        printf("SHA inode %d:",inr);
        if(inode.i_mode & IFDIR) {
            printf("No SHA for directories.\n");
        } else {
            for(int i=0; i<size_file; i+=SECTOR_SIZE) {
                filev6_readblock(&file, my_sector);
                for(int j=0; j<SECTOR_SIZE; ++j) {
                    if((j+index)<size_file) {
                        val[j+index]=my_sector[j];
                    }
                }
                index+=SECTOR_SIZE;
            }
            print_sha_from_content((const unsigned char *)val, size_file);
            printf("\n");
        }
    }
}
