/*
 * superblock operations for HAMMER Filesystem
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/errno.h>
#include <linux/string.h>

#include "dfly_wrap.h"
#include <vfs/hammer/hammer.h>

static struct file_system_type hammerfs_type;
static struct super_operations hammerfs_super_operations;

// corresponds to hammer_vfs_mount
static int hammerfs_fill_super(struct super_block *sb, void *data, int silent)
{
    hammer_mount_t hmp;
    hammer_volume_t rootvol;
    struct vnode *devvp = NULL;
    int error = -EINVAL;
    int maxinodes;

    /*
     * Internal mount data structure
     */
    hmp = kzalloc(sizeof(hmp), GFP_KERNEL);
    if (!hmp)
        return -ENOMEM;

    sb->s_fs_info = hmp;

    /*
     * Make sure kmalloc type limits are set appropriately.  If root
     * increases the vnode limit you may have to do a dummy remount
     * to adjust the HAMMER inode limit.
     */
// TODO
//    kmalloc_create(&hmp->m_misc, "HAMMER-others");
//    kmalloc_create(&hmp->m_inodes, "HAMMER-inodes");
   
    maxinodes = desiredvnodes + desiredvnodes / 5 +
            HAMMER_RECLAIM_WAIT;

// TODO
//        kmalloc_raise_limit(hmp->m_inodes,
//                    maxinodes * sizeof(struct hammer_inode));

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
    error = hammer_install_volume(hmp, sb->s_id, devvp);

    /*
     * Make sure we found a root volume
     */
    if (error == 0 && hmp->rootvol == NULL) {
        printk(KERN_ERR "HAMMER: No root volume found!\n");
        error = EINVAL;
    }

    /*
     * Check that all required volumes are available
     */
    if (error == 0 && hammer_mountcheck_volumes(hmp)) {
        printk(KERN_ERR "HAMMER: Missing volumes, cannot mount!\n");
        error = EINVAL;
    }

    if (error) {
        // TODO hammer_free_hmp(mp);
        return (error);
    }

    sb->s_op = &hammerfs_super_operations;
    sb->s_type = &hammerfs_type;

    /*
     * The root volume's ondisk pointer is only valid if we hold a
     * reference to it.
     */
    rootvol = hammer_get_root_volume(hmp, &error);
    if (error)
        goto failed;

    /*
    hammerfs_root_inode = new_inode(sb);
    hammerfs_root_inode->i_op = &hammerfs_inode_ops;
    hammerfs_root_inode->i_fop = &hammerfs_file_ops;
    hammerfs_root_inode->i_ino = 123;
    hammerfs_root_inode->i_mode = S_IFDIR | 0755;

    sb->s_root = d_alloc_root(hammerfs_root_inode);
    */

    printk(KERN_INFO "HAMMER: %s: mounted filesystem\n", sb->s_id);
    return(0);

failed:
    /*
     * Cleanup and return.
     */
    if (error)
        ; // TODO hammer_free_hmp(mp);
    return(error);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
struct super_block * hammerfs_get_sb(struct file_system_type *fs_type,
        int flags, const char *dev_name, void *data)
{
    return get_sb_nodev(fs_type, flags, data, hammerfs_fill_super);
}
#else
int hammerfs_get_sb(struct file_system_type *fs_type,
        int flags, const char *dev_name, void *data, struct vfsmount *mnt)
{
    return get_sb_nodev(fs_type, flags, data, hammerfs_fill_super, mnt);
}
#endif

int hammerfs_statfs(struct dentry * dentry, struct kstatfs * kstatfs)
{
    printk("hammerfs_statfs\n");
    return -ENOMEM;
}

static struct file_system_type hammerfs_type = {
    .owner   = THIS_MODULE,
    .name    = "hammer",
    .get_sb  = hammerfs_get_sb,
    .kill_sb = kill_anon_super
};

static struct super_operations hammerfs_super_operations = {
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
