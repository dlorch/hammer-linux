#include "dfly_wrap.h"
#include <linux/errno.h>

// from sys/sysctl.h
int desiredvnodes = KERN_MAXVNODES; // Maximum number of vnodes

// from kern/vfs_nlookup.c
int nlookup_init(struct nlookupdata *nd, const char *path, enum uio_seg seg, int flags) {
    return ENOENT;
}

int nlookup(struct nlookupdata *nd) {
    return ENOENT;
}

void nlookup_done(struct nlookupdata *nd) {}

// from kern/vfs_subr.c
int count_udev (int x, int y) {
    return 0;
}

int vfs_mountedon(struct vnode *vp) {
    return 0;
}

int vinvalbuf(struct vnode *vp, int flags, int slpflag, int slptimeo) {
    return 0;
}

int vn_isdisk(struct vnode *vp, int *errp) {
    return 1;
}

int vn_lock(struct vnode *vp, int flags) {
    return ENOENT;
}

void vn_unlock(struct vnode *vp) {}

// from kern/vopops.c
int vop_open(struct vop_ops *ops, struct vnode *vp, int mode, struct ucred *cred,
             struct file *fp) {
    return 0;
}

int vop_close(struct vop_ops *ops, struct vnode *vp, int fflag) {
    return 0;
}

int vop_fsync(struct vop_ops *ops, struct vnode *vp, int waitfor) {
    return 0;
}

// from kern/vfs_lock.c
void vrele(struct vnode *vp) {}

// from kern/vfs_cache.c
int cache_vref(struct nchandle *nch, struct ucred *cred, struct vnode **vpp) {
    return ENOENT;
}

// from platform/*/*/db_interface.c
void Debugger (const char *msg) {}

// from libkern/bcmp.c
int bcmp(const void *b1, const void *b2, size_t length) {
    return 0;
}

// from kern/vfs_bio.c
int bread(struct vnode *vp, off_t loffset, int size, struct buf **bpp) {
    return 0;
}

void brelse(struct buf *bp) {}

// from ??
void bzero (volatile void *buf, size_t len) {}
void bcopy (volatile const void *from, volatile void *to, size_t len) {}

// from kern/vfs_mount.c
int vmntvnodescan(
    struct mount *mp, 
    int flags,
    int (*fastfunc)(struct mount *mp, struct vnode *vp, void *data),
    int (*slowfunc)(struct mount *mp, struct vnode *vp, void *data),
    void *data
) {
    return 0;
}

// from kern/kern_slaballoc.c
void dfly_kfree(void *ptr, struct malloc_type *type) {}

void *dfly_kmalloc(unsigned long size, struct malloc_type *type, int flags) {
    return 0;
}
