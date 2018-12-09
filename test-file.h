#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "unixv6fs.h"
#include "inode.h"
#include "error.h"
#include "filev6.h"
#include "sha.h"

#define TAILLE SECTOR_SIZE+1

void test_print(struct unix_filesystem *u);
