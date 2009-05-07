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
#include "dfly_wrap.h"

static struct inode_operations hammerfs_inode_ops;
static struct file_operations hammerfs_file_ops;

static int hammerfs_open(struct inode * inode, struct file * file)
{
    printk("proc_hammer_open(node->i_ino=%lu)\n", inode->i_ino);
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

int hammerfs_readdir(struct file * file, void * dirent, filldir_t filldir)
{
    //struct dentry *de = file->f_dentry;

    printk("hammerfs_readdir(file->f_pos=%d)\n", file->f_pos);

    /*
    if(file->f_pos > 2)
        return 1;
    if(filldir(dirent, ".", 1, file->f_pos++, de->d_inode->i_ino, DT_DIR))
        return 0;
    if(filldir(dirent, "..", 2, file->f_pos++, de->d_parent->d_inode->i_ino, DT_DIR))
        return 0;
    if(filldir(dirent, "hammertime", strlen("hammertime"), file->f_pos++, 456, DT_REG))
        return 0;
        */

    return 1;
}

struct dentry * hammerfs_inode_lookup(struct inode * parent_inode, struct dentry * dentry, struct nameidata * nameidata)
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

static struct file_operations hammerfs_file_operations = {
    .owner = THIS_MODULE,
    .open = hammerfs_open,
    .read = hammerfs_read,
    .readdir = hammerfs_readdir
};
