/*
 * inode operations for HAMMER Filesystem
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/string.h>
#include "hammerfs.h"

#include "dfly_wrap.h"
#include <vfs/hammer/hammer.h>

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
 * read from disk (HAMMER).
 */
// corresponds to hammer_get_vnode and hammer_vop_getattr
int hammerfs_get_inode(struct super_block *sb,
                       struct hammer_inode *ip,
                       struct inode **inode) {
    int error;

    (*inode) = new_inode(sb);
    (*inode)->i_op = &hammerfs_inode_operations;
    (*inode)->i_fop = &hammerfs_file_operations;
    (*inode)->i_ino = ip->ino_leaf.base.obj_id;
    (*inode)->i_uid = hammer_to_unix_xid(&ip->ino_data.uid);
    (*inode)->i_gid = hammer_to_unix_xid(&ip->ino_data.gid);
    (*inode)->i_nlink = ip->ino_data.nlinks;
    (*inode)->i_size = ip->ino_data.size;
    (*inode)->i_mode = ip->ino_data.mode;

    switch(ip->ino_data.obj_type) {
    case HAMMER_OBJTYPE_DIRECTORY:
        (*inode)->i_mode |= S_IFDIR;
        break;
    case HAMMER_OBJTYPE_REGFILE:
        (*inode)->i_mode |= S_IFREG;
        break;
    case HAMMER_OBJTYPE_FIFO:
        (*inode)->i_mode |= S_IFIFO;
        break;
    case HAMMER_OBJTYPE_CDEV:
        (*inode)->i_mode |= S_IFCHR;
        break;
    case HAMMER_OBJTYPE_BDEV:
        (*inode)->i_mode |= S_IFBLK;
        break;
    case HAMMER_OBJTYPE_SOFTLINK:
        (*inode)->i_mode |= S_IFLNK;
        break;
    case HAMMER_OBJTYPE_SOCKET:
        (*inode)->i_mode |= S_IFSOCK;
        break;
/*
    case HAMMER_OBJTYPE_PSEUDOFS
    case HAMMER_OBJTYPE_DBFILE
*/
    }

    /*
     * We must provide a consistent atime and mtime for snapshots
     * so people can do a 'tar cf - ... | md5' on them and get
     * consistent results.
     */

    if(ip->flags & HAMMER_INODE_RO)  {
        hammer_time_to_timespec(ip->ino_data.ctime, &(*inode)->i_atime);
        hammer_time_to_timespec(ip->ino_data.ctime, &(*inode)->i_mtime);
    } else {
        hammer_time_to_timespec(ip->ino_data.atime, &(*inode)->i_atime);
        hammer_time_to_timespec(ip->ino_data.mtime, &(*inode)->i_mtime);
    }
    hammer_time_to_timespec(ip->ino_data.ctime, &(*inode)->i_ctime);

/*
    i->i_rdev;
    i->i_blocks;
    i->i_blkbits;
*/
    return(0);
}
