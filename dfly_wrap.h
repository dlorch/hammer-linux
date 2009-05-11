#ifndef _DFLY_WRAP_H
#define _DFLY_WRAP_H

/* 
 * Header file providing compability "glue" between 
 * DragonFly BSD and Linux: Contains mostly dummy
 * definitions and no-op functions.
 *
 * Use as follows: First include linux headers, then
 * dfly_wrap.h, then dfly headers.
 */

#include <linux/types.h>  // for u_ont32_t, uint64_t
#include <asm/bug.h>      // for BUG_ON
#include <linux/time.h>   // for struct timespec
#include <linux/bio.h>    // for struct bio
#include <linux/kernel.h> // for printk

/*
 * required DragonFly BSD definitions
 */

// indicate we are in kernel
#define _KERNEL 1

// from sys/cdefs.h
#define __unused

// from sys/dirent.h
#define DT_DBF	15		/* database record file*/

// from sys/stat.h
#define S_IFDB	0110000		/* record access file */

// from sys/malloc.h
#define MALLOC_DECLARE(type)
#define M_WAITOK        0x0002  /* wait for resources / alloc from cache */
#define M_ZERO          0x0100  /* bzero() the allocation */
#define M_USE_RESERVE   0x0200  /* can eat into free list reserve */

#define kfree(addr, type) dfly_kfree(addr, type)
#define kmalloc(size, type, flags) dfly_kmalloc(size, type, flags)

struct malloc_type {};

void dfly_kfree (void *addr, struct malloc_type *type);
void *dfly_kmalloc (unsigned long size, struct malloc_type *type, int flags);

// from sys/ktr.h
#define KTR_INFO_MASTER_EXTERN(master)

// from sys/thread.h
#define crit_enter()
#define crit_exit()

struct thread {};
typedef struct thread *thread_t;

// from sys/types.h
typedef u_int32_t udev_t;         /* device number */
typedef uint64_t u_quad_t;        /* quads */

// from sys/param.h
#define MAXBSIZE        65536   /* must be power of 2 */

// from sys/time.h
struct krate {
    int freq;
    int ticks;
    int count;
};

// from sys/statvfs.h
struct statvfs {
    long    f_blocks;               /* total data blocks in file system */
};

// from sys/buf.h
struct buf {
    caddr_t b_data;                 /* Memory, superblocks, indirect etc. */
};
struct vnode;
int bread (struct vnode *, off_t, int, struct buf **);
void brelse (struct buf *);
struct buf_rb_tree {
    void    *rbh_root;
};

// from sys/mount.h
#define MNT_RDONLY      0x00000001      /* read only Filesystem */
#define MNT_WAIT        1       /* synchronously wait for I/O to complete */
#define MNT_NOWAIT      2       /* start all I/O, but do not wait for it */

struct statfs {
    long    f_blocks;               /* total data blocks in file system */
};
struct netexport {};
struct export_args {};
struct mount {
    int mnt_flag;               /* flags shared with user */    
    struct statfs   mnt_stat;               /* cache of Filesystem stats */
    struct statvfs  mnt_vstat;              /* extended stats */
};

int vfs_mountedon (struct vnode *);    /* is a vfs mounted on vp */

// from sys/uio.h
enum uio_seg {
    UIO_USERSPACE,          /* from user data space */
    UIO_SYSSPACE,           /* from system space */
    UIO_NOCOPY              /* don't copy, already in object */
};

// from sys/vfscache.h
struct vattr {};
enum vtype { VNON, VREG, VDIR, VBLK, VCHR, VLNK, VSOCK, VFIFO, VBAD, VDATABASE };

// from sys/vfsops.h
#define VOP_OPEN(vp, mode, cred, fp)                    \
        vop_open(*(vp)->v_ops, vp, mode, cred, fp)
#define VOP_CLOSE(vp, fflag)                            \
        vop_close(*(vp)->v_ops, vp, fflag)
#define VOP_FSYNC(vp, waitfor)                          \
        vop_fsync(*(vp)->v_ops, vp, waitfor)

struct vop_inactive_args {};
struct vop_reclaim_args {};
struct vop_ops {};
struct ucred;

int vop_open(struct vop_ops *ops, struct vnode *vp, int mode,
             struct ucred *cred, struct file *file);
int vop_close(struct vop_ops *ops, struct vnode *vp, int fflag);
int vop_fsync(struct vop_ops *ops, struct vnode *vp, int waitfor);

// sys/conf.h
#define si_mountpoint   __si_u.__si_disk.__sid_mountpoint

struct cdev {
   union {
        struct {
                struct mount *__sid_mountpoint;
        } __si_disk;
   } __si_u; 
};

int count_udev (int x, int y);

// from sys/vnode.h
#define VMSC_GETVP      0x01
#define VMSC_NOWAIT     0x10
#define VMSC_ONEPASS    0x20

#define V_SAVE          0x0001          /* vinvalbuf: sync file first */

#define v_umajor        v_un.vu_cdev.vu_umajor
#define v_uminor        v_un.vu_cdev.vu_uminor
#define v_rdev          v_un.vu_cdev.vu_cdevinfo

struct vnode {
    void    *v_data;                        /* private data for fs */
    struct  buf_rb_tree v_rbdirty_tree;     /* RB tree of dirty bufs */
    enum    vtype v_type;                   /* vnode type */
    struct  vop_ops **v_ops;                /* vnode operations vector */
    union {
        struct {
            int vu_umajor;      /* device number for attach */
            int vu_uminor;
            struct cdev *vu_cdevinfo; /* device (VCHR, VBLK) */
        } vu_cdev;
   } v_un;
};

int vinvalbuf (struct vnode *vp, int save, int slpflag, int slptimeo);
int vn_isdisk (struct vnode *vp, int *errp);
int vn_lock (struct vnode *vp, int flags);
void vn_unlock (struct vnode *vp);
void vrele (struct vnode *vp);
int vmntvnodescan(struct mount *mp, int flags,
                  int (*fastfunc)(struct mount *mp, struct vnode *vp, void *data),
                  int (*slowfunc)(struct mount *mp, struct vnode *vp, void *data),
                  void *data);

// from sys/ucred.h
struct ucred {};
#define FSCRED ((struct ucred *)-1)     /* Filesystem credential */

// from sys/namecache.h
struct nchandle {};
int cache_vref(struct nchandle *, struct ucred *, struct vnode **);

// from sys/nlookup.h
#define NLC_FOLLOW              0x00000001      /* follow leaf symlink */

struct nlookupdata {
    struct nchandle nl_nch;         /* start-point and result */
    struct ucred    *nl_cred;       /* credentials for nlookup */
};

int nlookup_init(struct nlookupdata *, const char *, enum uio_seg, int);
int nlookup(struct nlookupdata *);
void nlookup_done(struct nlookupdata *);

// from sys/systm.h
#define KKASSERT(exp) BUG_ON(!exp)
#define kprintf printk
void Debugger (const char *msg);
void bzero (volatile void *buf, size_t len);

// from kern/vfs_subr.c
#define KERN_MAXVNODES           5      /* int: max vnodes */

// from sys/sysctl.h
extern int desiredvnodes;

// from sys/errno.h
#define EFTYPE          79              /* Inappropriate file type or format */

// from sys/fcntl.h
#define FREAD           0x0001
#define FWRITE          0x0002

// sys/lock.h
#define LK_EXCLUSIVE    0x00000002      /* exclusive lock */
#define LK_RETRY        0x00020000 /* vn_lock: retry until locked */

// sys/libkern.h
int bcmp (const void *, const void *, size_t);

/*
 * conflicting Linux definitions
 */

// in linux/module.h
#undef LIST_HEAD

// in linux/rbtree.h
#undef RB_BLACK
#undef RB_RED
#undef RB_ROOT

#endif /* _DFLY_WRAP_H */
