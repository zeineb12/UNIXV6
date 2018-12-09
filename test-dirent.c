/**
 * @file test-dirent.c
 * @brief tests the direntv6.c functions
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "direntv6.h"
#include "error.h"

int test(struct unix_filesystem *u)
{
    char prefix[MAXPATHLEN_UV6+1];
    prefix[0]='\0';
    int err=0;
    if ((err=direntv6_print_tree(u,ROOT_INUMBER,prefix))<0) return err;
    
    return 0;
}
