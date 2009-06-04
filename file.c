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
#include <vfs/hammer/hammer.h>

static int hammerfs_open(struct inode *inode, struct file *file)
{
#if DEBUG
    printk("hammerfs_open(node->i_ino=%lu)\n", inode->i_ino);
#endif

    return 0;
}

// corresponds to hammer_vop_read
static ssize_t hammerfs_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
    hammer_mount_t hmp;
    struct buffer_head *bh;
    struct super_block *sb;
    struct hammer_transaction trans;
    struct hammer_cursor cursor;
    struct inode *inode;
    struct hammer_inode *ip;
    hammer_base_elm_t base;
    hammer_off_t disk_offset;
    int64_t rec_offset;
    int error;
    int boff;
    int n;
    int block_num;
    size_t len = 0;
    hammer_off_t zone2_offset;
    int vol_no;
    hammer_volume_t volume;
 
#if DEBUG
    printk("hammerfs_read(size=%d, ppos=%d)\n", (int)size, (int)*ppos);
#endif

    inode = file->f_path.dentry->d_inode;
    ip = (struct hammer_inode *)inode->i_private;
    sb = inode->i_sb;
    hmp = (hammer_mount_t)sb->s_fs_info;

    hammer_simple_transaction(&trans, ip->hmp);
    hammer_init_cursor(&trans, &cursor, &ip->cache[1], ip);

   /*
    * Key range (begin and end inclusive) to scan.  Note that the key's
    * stored in the actual records represent BASE+LEN, not BASE.  The
    * first record containing bio_offset will have a key > bio_offset.
    */
    cursor.key_beg.localization = ip->obj_localization +
                                  HAMMER_LOCALIZE_MISC;
    cursor.key_beg.obj_id = ip->obj_id;
    cursor.key_beg.create_tid = 0;
    cursor.key_beg.delete_tid = 0;
    cursor.key_beg.obj_type = 0;
    cursor.key_beg.key = *ppos + 1;
    cursor.asof = ip->obj_asof;
    cursor.flags |= HAMMER_CURSOR_ASOF;

    cursor.key_end = cursor.key_beg;
    KKASSERT(ip->ino_data.obj_type == HAMMER_OBJTYPE_REGFILE);
    
    cursor.key_beg.rec_type = HAMMER_RECTYPE_DATA;
    cursor.key_end.rec_type = HAMMER_RECTYPE_DATA;
    cursor.key_end.key = 0x7FFFFFFFFFFFFFFFLL;
    cursor.flags |= HAMMER_CURSOR_END_INCLUSIVE;

    error = hammer_ip_first(&cursor);

    if(error == 0) {
       /*
        * Get the base file offset of the record.  The key for
        * data records is (base + bytes) rather then (base).
        */
        base = &cursor.leaf->base; 
        rec_offset = base->key - cursor.leaf->data_len;

       /*
        * Calculate the data offset in the record and the number
        * of bytes we can copy.
        */
        disk_offset = cursor.leaf->data_offset + rec_offset + *ppos;
        len = min(min(BLOCK_SIZE, size), base->key - *ppos);

        zone2_offset = hammer_blockmap_lookup(hmp, disk_offset, &error);
        vol_no = HAMMER_VOL_DECODE(zone2_offset);
        volume = hammer_get_volume(hmp, vol_no, &error);

        disk_offset = volume->ondisk->vol_buf_beg + (zone2_offset & HAMMER_OFF_SHORT_MASK);

        // TODO: move this into hammerfs_io_read, also calculate data length
        block_num = disk_offset / BLOCK_SIZE;
        boff = disk_offset % BLOCK_SIZE;

       /*
        * Read from disk
        */    
        bh = sb_bread(sb, block_num);
        if(!bh) {
            error = -ENOMEM;
            goto failed;
        }
        copy_to_user(buf, bh->b_data + boff * sizeof(unsigned char), len);
        brelse(bh);
    }

    hammer_done_cursor(&cursor);
    hammer_done_transaction(&trans);

    *ppos += len;
    return len;

failed:
    return error;
}

// corresponds to hammer_vop_readdir
int hammerfs_readdir(struct file *file, void *dirent, filldir_t filldir)
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

/*            if (base->obj_id != de->d_inode->i_ino)
                panic("readdir: bad record at %p", cursor.node);
*/

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

// corresponds to hammer_vop_nresolve
struct dentry *hammerfs_inode_lookup(struct inode *parent_inode, struct dentry *dentry,
                                     struct nameidata *nameidata)
{
    struct hammer_transaction trans;
    struct super_block *sb;
    struct inode *inode;
    hammer_inode_t dip;
    hammer_inode_t ip;
    hammer_tid_t asof;
    struct hammer_cursor cursor;
    int64_t namekey;
    u_int32_t max_iterations;
    int64_t obj_id;
    int nlen;
    int flags;
    int error;
    u_int32_t localization;

    printk("hammerfs_inode_lookup(parent_inode->i_ino=%d, dentry->d_name.name=%s)\n",
        parent_inode->i_ino, dentry->d_name.name);

    sb = parent_inode->i_sb;
    dip = (hammer_inode_t)parent_inode->i_private;
    asof = dip->obj_asof;
    localization = dip->obj_localization;   /* for code consistency */
    nlen = dentry->d_name.len;
    flags = dip->flags & HAMMER_INODE_RO;

    hammer_simple_transaction(&trans, dip->hmp);

   /*
    * Calculate the namekey and setup the key range for the scan.  This
    * works kinda like a chained hash table where the lower 32 bits
    * of the namekey synthesize the chain.
    *
    * The key range is inclusive of both key_beg and key_end.
    */
    namekey = hammer_directory_namekey(dip, dentry->d_name.name, nlen,
                                       &max_iterations);

    error = hammer_init_cursor(&trans, &cursor, &dip->cache[1], dip);
    cursor.key_beg.localization = dip->obj_localization +
                                  HAMMER_LOCALIZE_MISC;
    cursor.key_beg.obj_id = dip->obj_id;
    cursor.key_beg.key = namekey;
    cursor.key_beg.create_tid = 0;
    cursor.key_beg.delete_tid = 0;
    cursor.key_beg.rec_type = HAMMER_RECTYPE_DIRENTRY;
    cursor.key_beg.obj_type = 0;

    cursor.key_end = cursor.key_beg;
    cursor.key_end.key += max_iterations;
    cursor.asof = asof;
    cursor.flags |= HAMMER_CURSOR_END_INCLUSIVE | HAMMER_CURSOR_ASOF;

   /*
    * Scan all matching records (the chain), locate the one matching
    * the requested path component.
    *
    * The hammer_ip_*() functions merge in-memory records with on-disk
    * records for the purposes of the search.
    */
    obj_id = 0;
    localization = HAMMER_DEF_LOCALIZATION;

    if (error == 0) {
            error = hammer_ip_first(&cursor);
            while (error == 0) {
                error = hammer_ip_resolve_data(&cursor);
                if (error)
                        break;
                if (nlen == cursor.leaf->data_len - HAMMER_ENTRY_NAME_OFF &&
                    bcmp(dentry->d_name.name, cursor.data->entry.name, nlen) == 0) {
                        obj_id = cursor.data->entry.obj_id;
                        localization = cursor.data->entry.localization;
                        break;
                }
                error = hammer_ip_next(&cursor);
        }
    }
    hammer_done_cursor(&cursor);
    if (error == 0) {
        ip = hammer_get_inode(&trans, dip, obj_id,
                              asof, localization,
                              flags, &error); 
        if(error == 0) {
            error = hammerfs_get_inode(sb, ip, &inode);
            /* hammer_rel_inode(ip, 0); */        
        } else {
            ip = NULL;
        }
        if(error == 0) {
            d_add(dentry, inode);
        }
        goto done;
    }
done:
    /*hammer_done_transaction(&trans);*/
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
