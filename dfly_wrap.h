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
#include <linux/kernel.h> // for printk, simple_strtoul
#include <linux/ctype.h>  // for isascii, isdigit, isalpha, isupper, isspace
#include <linux/slab.h>   // for kmalloc
#include <linux/string.h> // for memcmp, memcpy, memset
#include <linux/buffer_head.h> // for brelse

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
#define UF_NOHISTORY    0x00000040      /* do not retain history/snapshots */
#define SF_NOHISTORY    0x00400000      /* do not retain history/snapshots */

// from cpu/i386/include/param.h
#define SMP_MAXCPU      16

// from sys/malloc.h
struct malloc_type {
    struct malloc_type *ks_next;    /* next in list */
    long    ks_memuse[SMP_MAXCPU];  /* total memory held in bytes */
    long    ks_loosememuse;         /* (inaccurate) aggregate memuse */
    long    ks_limit;       /* most that are allowed to exist */
    long    ks_size;        /* sizes of this thing that are allocated */
    long    ks_inuse[SMP_MAXCPU]; /* # of allocs currently in use */
    int64_t ks_calls;     /* total packets of this type ever allocated */
    long    ks_maxused;     /* maximum number ever used */
    uint32_t ks_magic;    /* if it's not magic, don't touch it */
    const char *ks_shortdesc;       /* short description */
    uint16_t ks_limblocks; /* number of times blocked for hitting limit */
    uint16_t ks_mapblocks; /* number of times blocked for kernel map */
    long    ks_reserved[4]; /* future use (module compatibility) */
};

#define M_MAGIC         877983977       /* time when first defined :-) */
#define MALLOC_DECLARE(type) \
    extern struct malloc_type type[1]
#define MALLOC_DEFINE(type, shortdesc, longdesc)        \
    struct malloc_type type[1] = {                  \
        { NULL, { 0 }, 0, 0, 0, { 0 }, 0, 0, M_MAGIC, shortdesc, 0, 0 } \
    };
#define M_WAITOK        0x0002  /* wait for resources / alloc from cache */
#define M_ZERO          0x0100  /* bzero() the allocation */
#define M_USE_RESERVE   0x0200  /* can eat into free list reserve */

#define kfree(addr, type) dfly_kfree(addr, type)
#define kmalloc(size, type, flags) dfly_kmalloc(size, type, flags)

MALLOC_DECLARE(M_TEMP);

void dfly_kfree (void *addr, struct malloc_type *type);
void *dfly_kmalloc (unsigned long size, struct malloc_type *type, int flags);

// from sys/ktr.h
#define KTR_INFO_MASTER_EXTERN(master)

// from sys/proc.h
#define PRISON_ROOT     0x1

struct lwp {};

// from sys/thread.h
#define crit_enter()
#define crit_exit()

struct thread {
    struct lwp  *td_lwp;        /* (optional) associated lwp */
};
typedef struct thread *thread_t;

extern int  lwkt_create (void (*func)(void *), void *, struct thread **,
                         struct thread *, int, int, const char *, ...);
extern void lwkt_exit (void);

// from platform/pc32/include/thread.h
#define curthread   ((thread_t)NULL)

// from sys/types.h
typedef u_int32_t udev_t;         /* device number */
typedef uint64_t u_quad_t;        /* quads */

// from sys/param.h
#define MAXBSIZE        65536   /* must be power of 2 */

#define PCATCH          0x00000100      /* tsleep checks signals */

// from sys/time.h
extern time_t   time_second;

struct krate {
    int freq;
    int ticks;
    int count;
};

void getmicrotime (struct timeval *tv);

// from sys/statvfs.h
struct statvfs {
    long    f_blocks;               /* total data blocks in file system */
};

// from sys/buf.h
struct buf {
    caddr_t b_data;                 /* Memory, superblocks, indirect etc. */
};
struct vnode;
int bread (struct super_block*, off_t, int, struct buf **);
#ifndef _LINUX_BUFFER_HEAD_H
void brelse (struct buf *);
#endif
void dfly_brelse (struct buf *);
struct buf_rb_tree {
    void    *rbh_root;
};
int     bd_heatup (void);

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

#define VINACTIVE       0x04000 /* The vnode is inactive (did VOP_INACTIVE) */

struct vnode {
    int     v_flag;                         /* vnode flags (see below) */
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

    struct super_block *sb; // defined by us, we use this for sb_bread()
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

// from cpu/*/*/stdarg.h
typedef __builtin_va_list   __va_list;  /* internally known to gcc */
#define __va_start(ap, last) \
        __builtin_va_start(ap, last)
#define __va_end(ap) \
        __builtin_va_end(ap)

// from sys/systm.h
#define KKASSERT(exp) BUG_ON(!(exp))
#define KASSERT(exp,msg) BUG_ON(!(exp))
#define kprintf printk
#define ksnprintf snprintf
#define strtoul simple_strtoul
#define bcopy memcpy
#define bzero(buf, len) memset(buf, 0, len)
void Debugger (const char *msg);
uint32_t crc32(const void *buf, size_t size);
uint32_t crc32_ext(const void *buf, size_t size, uint32_t ocrc);
int tsleep (void *, int, const char *, int);
void wakeup (void *chan);
int copyin (const void *udaddr, void *kaddr, size_t len);
int copyout (const void *kaddr, void *udaddr, size_t len);
u_quad_t strtouq (const char *, char **, int);
int kvprintf (const char *, __va_list);

// from kern/vfs_subr.c
#define KERN_MAXVNODES           5      /* int: max vnodes */

// from sys/sysctl.h
extern int desiredvnodes;

// from sys/errno.h
#define EFTYPE          79              /* Inappropriate file type or format */

// from sys/fcntl.h
#define FREAD           0x0001
#define FWRITE          0x0002

// from sys/lock.h
#define LK_EXCLUSIVE    0x00000002      /* exclusive lock */
#define LK_RETRY        0x00020000 /* vn_lock: retry until locked */

// from sys/libkern.h
#define bcmp(cs, ct, count) memcmp(cs, ct, count)

// from cpu/i386/include/param.h
#define MAXPHYS         (128 * 1024)    /* max raw I/O transfer size */

// from sys/signal2.h
#define CURSIG(lp)              __cursig(lp, 1, 0)
int __cursig(struct lwp *, int, int);

// from sys/buf.h
extern int      hidirtybufspace;

// from sys/kernel.h
extern int hz;                          /* system clock's frequency */

// from sys/iosched.h
void bwillwrite(int bytes);

// from sys/priv.h
#define PRIV_ROOT       1       /* Catch-all during development. */

int priv_check_cred(struct ucred *cred, int priv, int flags);

// from cpu/i386/include/limits.h
#define UQUAD_MAX       ULLONG_MAX      /* max value for a uquad_t */

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
