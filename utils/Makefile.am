AM_CFLAGS = -I../include -g3 -DDEBUG -D_LARGEFILE64_SOURCE

# Link the utility with the library
mkfs_sffs_LDADD = ../src/libsffs.la
bin_PROGRAMS = mkfs.sffs
mkfs_sffs_SOURCES = sffs_mkfs.c