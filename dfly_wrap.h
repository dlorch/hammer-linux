#ifndef _DFLY_WRAP_H
#define _DFLY_WRAP_H

#include <linux/types.h> // for u_ont32_t, uint64_t
#include <asm/bug.h>     // for BUG_ON
#include <linux/time.h>  // for struct timespec
#include <linux/bio.h>   // for struct bio

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

// from sys/ktr.h
#define KTR_INFO_MASTER_EXTERN(master)

// from sys/thread.h
struct thread {}; // TODO
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

// from sys/mount.h
struct netexport {}; // TODO
struct export_args {}; // TODO

// from sys/systm.h
#define KKASSERT(exp) BUG_ON(!exp)

// from sys/vfsops.h
struct vop_inactive_args {}; // TODO
struct vop_reclaim_args {}; // TODO

// from sys/ucred.h
struct ucred {}; // TODO

// from sys/vfscache.h
struct vattr {}; // TODO

// from kern/vfs_subr.c
#define KERN_MAXVNODES           5      /* int: max vnodes */

// from sys/sysctl.h
extern int desiredvnodes;

/*
 * conflicting Linux definitions
 */

// in include/linux/module.h
#undef LIST_HEAD

#endif /* _DFLY_WRAP_H */
