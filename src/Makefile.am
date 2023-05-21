# Both compiler and linker flags for FUSE
FUSE_C_FLAGS = -D_FILE_OFFSET_BITS=64 -I/usr/include/fuse
FUSE_LD_FLAGS = -lfuse -lpthread
AM_CFLAGS = -I../include -fPIC -g3 -DDEBUG -D_LARGEFILE64_SOURCE $(FUSE_C_FLAGS)

lib_LTLIBRARIES = libsffs.la
libsffs_la_SOURCES = sffs.c sffs_fuse.c sffs_device.c sffs_direntry.c err.c bitmaps.c