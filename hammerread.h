#ifndef _HAMMERREAD_H_
#define _HAMMERREAD_H_

#include "hammer_disk.h"

#ifndef BOOT2
struct blockentry {
    hammer_off_t    off;
    int     use;
    char        *data;
};

#ifdef TESTING
#define NUMCACHE    16
#else
#define NUMCACHE    6
#endif

struct hfs {
#ifdef TESTING
    int     fd;
#else   // libstand
    struct open_file *f;
#endif
    hammer_off_t    root;
    int64_t     buf_beg;
    int     lru;
    struct blockentry cache[NUMCACHE];
};
#endif /* #ifndef BOOT2 */

int hinit(struct hfs *);
int hstat(struct hfs *, ino_t, struct stat *);
ino_t hlookup(struct hfs *hfs, const char *path);
int hreaddir(struct hfs *, ino_t, int64_t *, struct dirent *);
ssize_t hreadf(struct hfs *, ino_t, int64_t, int64_t, char *);

#endif /* _HAMMERREAD_H_ */
