/*
 * superblock operations for HAMMER Filesystem
 */

#include <linux/module.h>
#include <linux/fs.h>          // for BLOCK_SIZE
#include <linux/version.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/buffer_head.h> // for sb_bread
#include "hammerfs.h"

#include "dfly_wrap.h"
#include <vfs/hammer/hammer.h>

// from vfs/hammer/hammer_vfsops.c
int hammer_debug_io;
int hammer_debug_general;
int hammer_debug_debug = 1;     /* medium-error panics */
int hammer_debug_inode;
int hammer_debug_locks;
int hammer_debug_btree;
int hammer_debug_tid;
int hammer_debug_recover;       /* -1 will disable, +1 will force */
int hammer_debug_recover_faults;
int hammer_cluster_enable = 1;      /* enable read clustering by default */
int hammer_count_fsyncs;
int hammer_count_inodes;
int hammer_count_iqueued;
int hammer_count_reclaiming;
int hammer_count_records;
int hammer_count_record_datas;
int hammer_count_volumes;
int hammer_count_buffers;
int hammer_count_nodes;
int64_t hammer_count_extra_space_used;
int64_t hammer_stats_btree_lookups;
int64_t hammer_stats_btree_searches;
int64_t hammer_stats_btree_inserts;
int64_t hammer_stats_btree_deletes;
int64_t hammer_stats_btree_elements;
int64_t hammer_stats_btree_splits;
int64_t hammer_stats_btree_iterations;
int64_t hammer_stats_record_iterations;

int64_t hammer_stats_file_read;
int64_t hammer_stats_file_write;
int64_t hammer_stats_file_iopsr;
int64_t hammer_stats_file_iopsw;
int64_t hammer_stats_disk_read;
int64_t hammer_stats_disk_write;
int64_t hammer_stats_inode_flushes;
int64_t hammer_stats_commits;

int hammer_count_dirtybufspace;     /* global */
int hammer_count_refedbufs;     /* global */
int hammer_count_reservations;
int hammer_count_io_running_read;
int hammer_count_io_running_write;
int hammer_count_io_locked;
int hammer_limit_dirtybufspace;     /* per-mount */
int hammer_limit_recs;          /* as a whole XXX */
int hammer_autoflush = 2000;        /* auto flush */
int hammer_bio_count;
int hammer_verify_zone;
int hammer_verify_data = 1;
int hammer_write_mode;
int64_t hammer_contention_count;
int64_t hammer_zone_limit;

static int hammerfs_install_volume(struct hammer_mount *hmp, struct super_block *sb);
struct inode *hammerfs_iget(struct super_block *sb, ino_t ino);

// corresponds to hammer_vfs_mount
static int
hammerfs_fill_super(struct super_block *sb, void *data, int silent)
{
    hammer_mount_t hmp;
    struct inode *root;
    int error = -EINVAL;

    /*
     * Internal mount data structure
     */
    hmp = kzalloc(sizeof(struct hammer_mount), GFP_KERNEL);
    if (!hmp)
        return -ENOMEM;

    sb->s_fs_info = hmp;

    if(!sb_set_blocksize(sb, BLOCK_SIZE)) {
        printk(KERN_ERR "HAMMER: %s: bad blocksize %d\n",
            sb->s_id, BLOCK_SIZE);
        goto failed;
    }

    hmp->root_btree_beg.localization = 0x00000000U;
    hmp->root_btree_beg.obj_id = -0x8000000000000000LL;
    hmp->root_btree_beg.key = -0x8000000000000000LL;
    hmp->root_btree_beg.create_tid = 1;
    hmp->root_btree_beg.delete_tid = 1;
    hmp->root_btree_beg.rec_type = 0;
    hmp->root_btree_beg.obj_type = 0;

    hmp->root_btree_end.localization = 0xFFFFFFFFU;
    hmp->root_btree_end.obj_id = 0x7FFFFFFFFFFFFFFFLL;
    hmp->root_btree_end.key = 0x7FFFFFFFFFFFFFFFLL;
    hmp->root_btree_end.create_tid = 0xFFFFFFFFFFFFFFFFULL;
    hmp->root_btree_end.delete_tid = 0;   /* special case */
    hmp->root_btree_end.rec_type = 0xFFFFU;
    hmp->root_btree_end.obj_type = 0;

    hmp->krate.freq = 1;    /* maximum reporting rate (hz) */
    hmp->krate.count = -16; /* initial burst */

    hmp->sync_lock.refs = 1;
    hmp->free_lock.refs = 1;
    hmp->undo_lock.refs = 1;
    hmp->blkmap_lock.refs = 1;

    TAILQ_INIT(&hmp->delay_list);
    TAILQ_INIT(&hmp->flush_group_list);
    TAILQ_INIT(&hmp->objid_cache_list);
    TAILQ_INIT(&hmp->undo_lru_list);
    TAILQ_INIT(&hmp->reclaim_list);

    hmp->master_id = 0;

    hmp->asof = HAMMER_MAX_TID;

    RB_INIT(&hmp->rb_vols_root);
    RB_INIT(&hmp->rb_inos_root);
    RB_INIT(&hmp->rb_nods_root);
    RB_INIT(&hmp->rb_undo_root);
    RB_INIT(&hmp->rb_resv_root);
    RB_INIT(&hmp->rb_bufs_root);
    RB_INIT(&hmp->rb_pfsm_root);

    hmp->ronly = 1;

    TAILQ_INIT(&hmp->volu_list);
    TAILQ_INIT(&hmp->undo_list);
    TAILQ_INIT(&hmp->data_list);
    TAILQ_INIT(&hmp->meta_list);
    TAILQ_INIT(&hmp->lose_list);

    /*
     * Load volumes
     */
    error = hammerfs_install_volume(hmp, sb);

    /*
     * Make sure we found a root volume
     */
    if (error == 0 && hmp->rootvol == NULL) {
        printk(KERN_ERR "HAMMER: No root volume found!\n");
        error = -EINVAL;
    }

    /*
     * Check that all required volumes are available
     */
    if (error == 0 && hammer_mountcheck_volumes(hmp)) {
        printk(KERN_ERR "HAMMER: Missing volumes, cannot mount!\n");
        error = -EINVAL;
    }

    if (error) {
        goto failed;
    }

    /*
     * Set super block operations
     */
    sb->s_op = &hammerfs_super_operations;
    sb->s_type = &hammerfs_type;

   /*
    * Locate the root directory using the root cluster's B-Tree as a
    * starting point.  The root directory uses an obj_id of 1.
    */
    root = hammerfs_iget(sb, HAMMER_OBJID_ROOT);
    if (IS_ERR(root)) {
        printk(KERN_ERR "HAMMER: get root inode failed\n");
        error = PTR_ERR(root);
        goto failed;
    }

    sb->s_root = d_alloc_root(root);
    if (!sb->s_root) {
        printk(KERN_ERR "HAMMER: get root dentry failed\n");
        iput(root);
        error = -ENOMEM;
        goto failed;
    }

    printk(KERN_INFO "HAMMER: %s: mounted filesystem\n", sb->s_id);
    return(0);

failed:
    return(error);
}

/**
 * Load a HAMMER volume by name.  Returns 0 on success or a positive error
 * code on failure.
 */
// corresponds to hammer_install_volume
static int
hammerfs_install_volume(struct hammer_mount *hmp, struct super_block *sb) {
    struct buffer_head * bh;
    hammer_volume_t volume;
    struct hammer_volume_ondisk *ondisk;
    int error = 0;

    /*
     * Allocate a volume structure
     */
    ++hammer_count_volumes;
    volume = kzalloc(sizeof(struct hammer_volume), GFP_KERNEL);
    volume->vol_name = kstrdup(sb->s_id, GFP_KERNEL);
    volume->io.hmp = hmp;   /* bootstrap */
    volume->io.offset = 0LL;
    volume->io.bytes = HAMMER_BUFSIZE;

    volume->sb = sb;

    /*
     * Extract the volume number from the volume header and do various
     * sanity checks.
     */
    bh = sb_bread(sb, 0);
    if(!bh) {
        printk(KERN_ERR "HAMMER: %s: unable to read superblock\n", sb->s_id);
        error = -EINVAL;
        goto failed;
    }

    ondisk = (struct hammer_volume_ondisk *)bh->b_data;
    if (ondisk->vol_signature != HAMMER_FSBUF_VOLUME) {
        printk(KERN_ERR "hammer_mount: volume %s has an invalid header\n",
                volume->vol_name);
        error = -EINVAL;
        goto failed;
    }

    volume->ondisk = ondisk;
    volume->vol_no = ondisk->vol_no;
    volume->buffer_base = ondisk->vol_buf_beg;
    volume->vol_flags = ondisk->vol_flags;
    volume->nblocks = ondisk->vol_nblocks; 
    volume->maxbuf_off = HAMMER_ENCODE_RAW_BUFFER(volume->vol_no,
                                ondisk->vol_buf_end - ondisk->vol_buf_beg);
    volume->maxraw_off = ondisk->vol_buf_end;

    if (RB_EMPTY(&hmp->rb_vols_root)) {
        hmp->fsid = ondisk->vol_fsid;
    } else if (bcmp(&hmp->fsid, &ondisk->vol_fsid, sizeof(uuid_t))) {
        printk(KERN_ERR "hammer_mount: volume %s's fsid does not match "
                        "other volumes\n", volume->vol_name);
        error = -EINVAL;
        goto failed;
    }

    /*
     * Insert the volume structure into the red-black tree.
     */
    if (RB_INSERT(hammer_vol_rb_tree, &hmp->rb_vols_root, volume)) {
        printk(KERN_ERR "hammer_mount: volume %s has a duplicate vol_no %d\n",
            volume->vol_name, volume->vol_no);
        error = -EEXIST;
    }

    /*
     * Set the root volume .  HAMMER special cases rootvol the structure.
     * We do not hold a ref because this would prevent related I/O
     * from being flushed.
     */
    if (error == 0 && ondisk->vol_rootvol == ondisk->vol_no) {
        hmp->rootvol = volume;
        hmp->nvolumes = ondisk->vol_count;
    }

    return(0);

failed:
    if(bh)
        brelse(bh);
    return(error);
}

/*
 * Report critical errors.  ip may be NULL.
 */
// from vfs/hammer/hammer_vfsops.c
void
hammer_critical_error(hammer_mount_t hmp, hammer_inode_t ip,
              int error, const char *msg)
{
    printk(KERN_CRIT "HAMMER: Critical error %s\n", msg);
    hmp->error = error;
}

int hammerfs_get_sb(struct file_system_type *fs_type,
        int flags, const char *dev_name, void *data, struct vfsmount *mnt)
{
    return get_sb_bdev(fs_type, flags, dev_name, data, hammerfs_fill_super, mnt);
}

int hammerfs_statfs(struct dentry * dentry, struct kstatfs * kstatfs)
{
    return -ENOMEM;
}

struct file_system_type hammerfs_type = {
    .owner   = THIS_MODULE,
    .name    = "hammer",
    .get_sb  = hammerfs_get_sb,
    .kill_sb = kill_anon_super
};

struct super_operations hammerfs_super_operations = {
    .statfs  = hammerfs_statfs
};

// corresponds to hammer_vfs_init
static int __init init_hammerfs(void)
{
    return register_filesystem(&hammerfs_type);
}

static void __exit exit_hammerfs(void)
{
    unregister_filesystem(&hammerfs_type);
}

MODULE_DESCRIPTION("HAMMER Filesystem");
MODULE_AUTHOR("Matthew Dillon, Daniel Lorch");
MODULE_LICENSE("GPL");
module_init(init_hammerfs)
module_exit(exit_hammerfs)
