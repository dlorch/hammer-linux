# -DTESTING for binary
#
# Linux:
# -D_BSD_SOURCE for DT_FIFO, DT_CHR .. in <dirent.h>
# -D_FILE_OFFSET_BITS=64 for large ino_t in <dirent.h>
# -D_XOPEN_SOURCE=500 for pread64 in <unistd.h>
#
# OSX:
# -D_DARWIN_C_SOURCE for DT_FIFO, DT_CHR .. in <sys/dirent.h>
# -D__DARWIN_64_BIT_INO_T for large ino_t in <sys/dirent.h>

CFLAGS=-Wall -std=c99 -DTESTING

UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
	CFLAGS += -D_BSD_SOURCE -D_FILE_OFFSET_BITS=64 -D_XOPEN_SOURCE=500
endif

ifeq ($(UNAME), Darwin)
	CFLAGS += -D_DARWIN_C_SOURCE -D__DARWIN_64_BIT_INO_T
endif

hammerread: hammerread.c
