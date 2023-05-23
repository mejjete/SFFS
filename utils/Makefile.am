AM_CFLAGS = -I../include -I/usr/include/fuse -DDEBUG -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64

# mkfs.sffs utility 
bin_PROGRAMS = mkfs.sffs mount.sffs
mkfs_sffs_LDADD = ../src/libsffs.la
mkfs_sffs_SOURCES = sffs_mkfs.c

# mount.sffs utulity
mount_sffs_LDADD = -lfuse -lpthread ../src/libsffs.la
mount_sffs_SOURCES = sffs_mount.c

# umount.sffs utility
bin_SCRIPTS = umount.sffs