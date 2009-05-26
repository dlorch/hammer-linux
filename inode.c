/*
 * inode operations for HAMMER Filesystem
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/string.h>

#include "dfly_wrap.h"
#include <vfs/hammer/hammer.h>

static struct inode_operations hammerfs_inode_ops;
static struct file_operations hammerfs_file_ops;

int hammerfs_get_inode(struct super_block *sb, struct hammer_inode *ip, struct inode **inode);

// corresponds to hammer_vfs_vget
struct inode *hammerfs_iget(struct super_block *sb, ino_t ino) {
    struct hammer_transaction trans;
    struct hammer_mount *hmp = (void*)sb->s_fs_info;
    struct hammer_inode *ip;
    struct inode *inode;
    int error = 0;
   
    hammer_simple_transaction(&trans, hmp); 

   /*
    * Lookup the requested HAMMER inode.  The structure must be
    * left unlocked while we manipulate the related vnode to avoid
    * a deadlock.
    */
    ip = hammer_get_inode(&trans, NULL, ino,
                          hmp->asof, HAMMER_DEF_LOCALIZATION, 
                          0, &error);
    if (ip == NULL) {
        hammer_done_transaction(&trans);
        goto failed;
    }
    error = hammerfs_get_inode(sb, ip, &inode);
    hammer_rel_inode(ip, 0);
    hammer_done_transaction(&trans);

    return inode;
failed:
    iget_failed(inode);
    return ERR_PTR(error);
}

/*
 * Returns an in-memory inode (Linux VFS) corresponding to an inode
 * read from disk (HAMMER). The former are called vnodes in dfly.
 */
// corresponds to hammer_get_vnode
int hammerfs_get_inode(struct super_block *sb, struct hammer_inode *ip, struct inode **inode) {
    int error;

    (*inode) = new_inode(sb);
    (*inode)->i_op = &hammerfs_inode_ops;
    (*inode)->i_fop = &hammerfs_file_ops;
    (*inode)->i_ino = 42;
    (*inode)->i_mode = S_IFDIR | 0755;
 
    return(0);
}
