# -DTESTING for binary
# -D_BSD_SOURCE for DT_FIFO, DT_CHR .. in <dirent.h>
# -D_FILE_OFFSET_BITS=64 for large ino_t in <dirent.h>
# -D_XOPEN_SOURCE=500 for pread64 in <unistd.h>

CFLAGS=-Wall -std=c99 -DTESTING -D_BSD_SOURCE -D_FILE_OFFSET_BITS=64 -D_XOPEN_SOURCE=500

hammerread: hammerread.c
