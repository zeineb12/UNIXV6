/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall hello.c `pkg-config fuse --cflags --libs` -o hello
*/

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "unixv6fs.h"
#include "mount.h"
#include "sector.h"
#include "error.h"
#include "direntv6.h"
#include "inode.h"
#include "filev6.h"

struct unix_filesystem fs;

/**
 * @brief copies into buffer buf, the content of a file and prints it
 * @param path contains the name of the directory or file to read
 * @param stbuf pointer to the struct that we have to initialize
 * @return 0 on success; <0 on an error
 */

static int fs_getattr(const char *path, struct stat *stbuf)
{
    int erreur = 0;
    memset(stbuf, 0, sizeof(struct stat));
    /*We get the inode number of the file or directory*/
    int inode_nbr=0;
    if((inode_nbr = direntv6_dirlookup(&fs, ROOT_INUMBER, path))<0) return inode_nbr;
    struct inode ino;
    if((erreur = inode_read(&fs, (uint16_t)inode_nbr, &ino))!=0) {
        return erreur;
    }

    /*Initialisation of the struct stat stbuf*/
    stbuf->st_dev = 0;
    stbuf->st_ino = inode_nbr;
    if(ino.i_mode & IFDIR) {
        stbuf->st_mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH | S_IFDIR;
    } else {
        stbuf->st_mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH | S_IFREG;
    }
    stbuf->st_nlink = ino.i_nlink;
    stbuf->st_uid = ino.i_uid;
    stbuf->st_gid = ino.i_gid;
    stbuf->st_rdev = 0;
    stbuf->st_size = ((uint32_t)ino.i_size1)<<8 | (uint32_t)ino.i_size0;
    stbuf->st_blksize = SECTOR_SIZE;
    stbuf->st_blocks = inode_getsectorsize(&ino);
    stbuf->st_atim.tv_sec = 0;
    stbuf->st_atim.tv_nsec = 0;
    stbuf->st_mtim.tv_sec = 0;
    stbuf->st_mtim.tv_nsec = 0;
    stbuf->st_ctim.tv_sec = 0;
    stbuf->st_ctim.tv_nsec = 0;

    return 0;
}

/**
 * @brief copies into buffer buf, the names of every element
 * contained inside the directory corresponding to path
 * @param path contains the path of the directory to read
 * @param buf contains the names of all the elements contained in the directory(OUT)
 * @param filler method assimilated to memcpy
 * @param offset to be ignored
 * @param fi to be ignored
 * @return 0 on success; or <0 on an error
 */

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    M_REQUIRE_NON_NULL(path);
    M_REQUIRE_NON_NULL(buf);
    int inr =0;
    if((inr = direntv6_dirlookup(&fs, ROOT_INUMBER, path))<0)return inr;

    struct directory_reader d;
    int err=0;
    if ((err=direntv6_opendir(&fs,inr,&d))<0) return err;
    char name[DIRENT_MAXLEN+1];
    name[DIRENT_MAXLEN] = '\0';
    uint16_t child_inr;
    while((err=direntv6_readdir(&d, name, &child_inr))==1) {
        filler(buf,name,NULL,0);
    }
    return err;
}

/**
 * @brief copies into buffer buf, the content of a file and prints it
 * @param path contains the path of the file to read
 * @param buf contains the content of the file(OUT)
 * @param size contains the bytes read in the previous call to the function
 * @param offset position from where we need to read the file
 * @param fi to be ignored
 * @return the number of bytes read on success; 0 on an error
 */

static int fs_read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi)
{
    (void) fi;
    struct filev6 fv6;
    int inode_nbr = direntv6_dirlookup(&fs, ROOT_INUMBER, path);
    /* An error has occured so we cannot read any byte */
    if(inode_nbr<ROOT_INUMBER) return 0;
    int erreur = filev6_open(&fs,(uint16_t)inode_nbr,&fv6);
    if(erreur < 0) return 0;
    else {
        /* Number of bytes read in total */
        int result=0;
        /* Number of bytes read in one call to readblock */
        int read =0;
        char secteur[SECTOR_SIZE];
        if((erreur = filev6_lseek(&fv6,offset))<0) {
            return 0;
        }
        read = filev6_readblock(&fv6,secteur);
        while((result < size) && (read!=0)) {
            if(read<0) return 0;
            memcpy(buf+result,secteur,read);
            result+=read;
            read = filev6_readblock(&fv6,secteur);
        }
        return result;
    }
}

static struct fuse_operations available_ops = {
    .getattr = fs_getattr,
    .readdir = fs_readdir,
    .read = fs_read,
};

/* From https://github.com/libfuse/libfuse/wiki/Option-Parsing.
 * This will look up into the args to search for the name of the FS.
 */

/**
* @brief mounts the filesystem
* @param data to be ignored
* @param filename name of the disk
* @param key
* @param outargs to be ignored
* @return 0 on success; <0 on an error
*/
static int arg_parse(void *data, const char *filename, int key, struct fuse_args *outargs)
{
    (void) data;
    (void) outargs;
    if (key == FUSE_OPT_KEY_NONOPT && fs.f == NULL && filename != NULL) {
        int err =0;
        err=mountv6(filename,&fs);
        if(err != 0) {
            puts(ERR_MESSAGES[err - ERR_FIRST]);
            exit(1);
        }
        return 0;
    }
    return 1;
}


int main(int argc, char *argv[])
{
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    int ret = fuse_opt_parse(&args, NULL, NULL, arg_parse);
    if (ret == 0) {
        ret = fuse_main(args.argc, args.argv, &available_ops, NULL);
        (void)umountv6(&fs);
    }
    return ret;
}
