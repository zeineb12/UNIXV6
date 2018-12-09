/**
 * @file test-inode.c
 * @brief tests the inode.c functions
 */

#include <stdio.h>
#include "test-inodes.h"
#include "inode.h"

int test(struct unix_filesystem *u)
{
    inode_scan_print(u);
    return 0;
}
