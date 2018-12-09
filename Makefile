CFLAGS+= -Wall -g 
CC = gcc

all: test-inodes test-file test-dirent shell fs test-bitmap test-write

test-inodes : test-core.o test-inodes.o mount.o error.o inode.o sector.o filev6.o bmblock.o -lm

test-file : test-core.o test-file.o mount.o error.o inode.o sector.o filev6.o sha.o -lcrypto bmblock.o -lm

test-dirent : test-core.o test-dirent.o mount.o error.o inode.o sector.o filev6.o direntv6.o bmblock.o -lm

shell : shell.o mount.o error.o inode.o sector.o filev6.o sha.o direntv6.o -lcrypto bmblock.o -lm

fs.o : fs.c
	$(COMPILE.c) -D_DEFAULT_SOURCE $$(pkg-config fuse --cflags) -o $@ -c $<

fs : fs.o mount.o sector.o direntv6.o inode.o filev6.o error.o bmblock.o -lm
	$(LINK.c) -o $@ $^ $(LDLIBS) $$(pkg-config fuse --libs)

test-bitmap : test-bitmap.o bmblock.o error.o -lm

test-write : test-core.o test-write.o mount.o error.o inode.o sector.o filev6.o bmblock.o -lm

clean:
	rm *.o
