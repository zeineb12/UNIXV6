# UNIXV6

### Group project for the System-Oriented Programming course by Prof. Chappelier and Prof. Bugnion where we had to implement a unix v6 filesystem.

The different steps of the project consist of:

1. Implementation of listing, reading and writing files from a disk formatted with the Unix v6 filesystem.

2. Implementation of a shell which supports the following commands:

* exit

* quit

* help

* mount

* lsall

* psb

* cat

* sha

* inode

* istat

* mkfs

* mkdir

* add

3. Integration with FUSE. (Note that you should install FUSE first : sudo apt-get install libfuse2 libfuse-dev)

4. Implementation of bitmap vectors and integrating it to the project (ibm is used for availability of inodes for writing and fbm for the availability of sectors)
