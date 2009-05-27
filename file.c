/*
 * file operations for HAMMER Filesystem
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/errno.h>
#include <linux/string.h>
#include "hammerfs.h"

#include "dfly_wrap.h"

static int hammerfs_open(struct inode * inode, struct file * file)
{
    printk("hammerfs_open(node->i_ino=%lu)\n", inode->i_ino);
    return 0;
}

static ssize_t hammerfs_read(struct file * file, char __user * buf, size_t size, loff_t * ppos)
{
    //size_t len;
    printk("hammerfs_read(size=%d, ppos=%d)\n", (int)size, (int)*ppos);
    /*
    if(*ppos == 0) {
      len = strlen(hammer_time[hammer_current_frame]);
      copy_to_user(buf, hammer_time[hammer_current_frame], len);
      *ppos += len;
      hammer_current_frame = (hammer_current_frame + 1) % HAMMER_FRAMES;
      return len;
    }
    */
    return 0;
}

// corresponds to hammer_vop_readdir
int hammerfs_readdir(struct file * file, void * dirent, filldir_t filldir)
{
    struct dentry *de = file->f_dentry;
    struct hammer_transaction trans;
    struct hammer_cursor cursor;
    struct hammer_inode *ip = (struct hammer_inode *)de->d_inode->i_private;
    hammer_base_elm_t base;
    int r;
    int error;
    int dtype;

    printk("hammerfs_readdir(file->f_pos=%d)\n", file->f_pos);

   /*
    * Handle artificial entries
    */

    if(file->f_pos == 0) {
        r = filldir(dirent, ".", 1, file->f_pos++, de->d_inode->i_ino, DT_DIR);
    }

    if(!r && file->f_pos == 1) {
        if(de->d_parent->d_inode) {
            r = filldir(dirent, "..", 2, file->f_pos++, de->d_parent->d_inode->i_ino, DT_DIR);
        } else {
            r = filldir(dirent, "..", 2, file->f_pos++, de->d_inode->i_ino, DT_DIR);
        }
    }

    if(!r) {
        hammer_simple_transaction(&trans, ip->hmp);

       /*
        * Key range (begin and end inclusive) to scan.  Directory keys
        * directly translate to a 64 bit 'seek' position.
        */
        hammer_init_cursor(&trans, &cursor, &ip->cache[1], ip);
        cursor.key_beg.localization = ip->obj_localization +
                                      HAMMER_LOCALIZE_MISC;
        cursor.key_beg.obj_id = ip->obj_id;
        cursor.key_beg.create_tid = 0;
        cursor.key_beg.delete_tid = 0;
        cursor.key_beg.rec_type = HAMMER_RECTYPE_DIRENTRY;
        cursor.key_beg.obj_type = 0;
        cursor.key_beg.key = file->f_pos;

        cursor.key_end = cursor.key_beg;
        cursor.key_end.key = HAMMER_MAX_KEY;
        cursor.asof = ip->obj_asof;
        cursor.flags |= HAMMER_CURSOR_END_INCLUSIVE | HAMMER_CURSOR_ASOF;

        error = hammer_ip_first(&cursor);

        while (error == 0) {
            error = hammer_ip_resolve_data(&cursor);
            if (error)
                break;
            base = &cursor.leaf->base;
            KKASSERT(cursor.leaf->data_len > HAMMER_ENTRY_NAME_OFF);

            if (base->obj_id != de->d_inode->i_ino)
                panic("readdir: bad record at %p", cursor.node);

            /*
             * Convert pseudo-filesystems into softlinks
            */
            dtype = hammerfs_get_itype(cursor.leaf->base.obj_type);
            r = filldir(dirent, (void *)cursor.data->entry.name,
                        cursor.leaf->data_len - HAMMER_ENTRY_NAME_OFF,
                        file->f_pos, cursor.data->entry.obj_id, dtype);

            if (r)
                break;
            file->f_pos++;
            error = hammer_ip_next(&cursor);
        }

        hammer_done_cursor(&cursor);
    }

done:
    /*hammer_done_transaction(&trans);*/
    return(1); // TODO
failed:
    return(1);
}

struct dentry *hammerfs_inode_lookup(struct inode *parent_inode, struct dentry *dentry,
                                     struct nameidata *nameidata)
{
//    struct inode * file_inode;

    printk("hammerfs_inode_lookup(parent_inode->i_ino=%d, dentry->d_name.name=%s)\n",
        parent_inode->i_ino, dentry->d_name.name);

   /*
   if(parent_inode->i_ino == hammerfs_root_inode->i_ino
       && !strncmp(dentry->d_name.name, "hammertime", dentry->d_name.len)) {

      file_inode = new_inode(parent_inode->i_sb);
      file_inode->i_ino = 456;
      file_inode->i_size = strlen(hammer_time[hammer_current_frame]); 
      file_inode->i_mode = S_IFREG | 0644;
      file_inode->i_op = &hammerfs_inode_ops;
      file_inode->i_fop = &hammerfs_file_ops;
      d_add(dentry, file_inode);
    }
    */

    return NULL;
}

struct file_operations hammerfs_file_operations = {
    .owner = THIS_MODULE,
    .open = hammerfs_open,
    .read = hammerfs_read,
    .readdir = hammerfs_readdir
};

int hammerfs_setattr(struct dentry *dentry, struct iattr *iattr)
{
    printk(KERN_INFO "hammerfs_setattr(ino=%lu, name=%s)\n",
                     dentry->d_inode->i_ino, dentry->d_name.name);

    return -EPERM;
}

int hammerfs_getattr(struct vfsmount *mnt, struct dentry *dentry,
                     struct kstat *stat)
{
    struct inode *inode;

    printk(KERN_INFO "hammerfs_getattr(ino=%lu, name=%s)\n",
                     dentry->d_inode->i_ino, dentry->d_name.name);

    inode = dentry->d_inode;
    generic_fillattr(inode, stat);

    return 0;
}

struct inode_operations hammerfs_inode_operations = {
    .lookup = hammerfs_inode_lookup,
    .setattr = hammerfs_setattr,
    .getattr = hammerfs_getattr
};
