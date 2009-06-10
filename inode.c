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
//    hammer_rel_inode(ip, 0);
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
    (*inode)->i_mapping->a_ops = &hammerfs_address_space_operations;
    (*inode)->i_ino = ip->ino_leaf.base.obj_id;
    (*inode)->i_uid = hammer_to_unix_xid(&ip->ino_data.uid);
    (*inode)->i_gid = hammer_to_unix_xid(&ip->ino_data.gid);
    (*inode)->i_nlink = ip->ino_data.nlinks;
    (*inode)->i_size = ip->ino_data.size;
    (*inode)->i_mode = ip->ino_data.mode | hammerfs_get_itype(ip->ino_data.obj_type);
    (*inode)->i_private = ip;

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

// corresponds to hammer_get_dtype
int hammerfs_get_itype(char obj_type)
{
    switch(obj_type) {
    case HAMMER_OBJTYPE_DIRECTORY:
        return(S_IFDIR);
    case HAMMER_OBJTYPE_REGFILE:
        return(S_IFREG);
    case HAMMER_OBJTYPE_FIFO:
        return(S_IFIFO);
    case HAMMER_OBJTYPE_CDEV:
        return(S_IFCHR);
    case HAMMER_OBJTYPE_BDEV:
        return(S_IFBLK);
    case HAMMER_OBJTYPE_SOFTLINK:
        return(S_IFLNK);
    case HAMMER_OBJTYPE_SOCKET:
        return(S_IFSOCK);
/*
    case HAMMER_OBJTYPE_PSEUDOFS
    case HAMMER_OBJTYPE_DBFILE
*/
    }
}

// corresponds to hammer_vop_strategy_read
int hammerfs_readpage(struct file *file, struct page *page) {
    void *page_addr;
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
    int64_t file_offset;
    int error = 0;
    int boff;
    int roff;
    int n;
    int i;
    int block_num;
    int block_offset;
    hammer_off_t zone2_offset;
    int vol_no;
    hammer_volume_t volume;

    printk ("hammerfs_readpage(page->index=%d)\n", (int) page->index);

    inode = file->f_path.dentry->d_inode;
    ip = (struct hammer_inode *)inode->i_private;
    sb = inode->i_sb;
    hmp = (hammer_mount_t)sb->s_fs_info;
    hammer_simple_transaction(&trans, ip->hmp);
    hammer_init_cursor(&trans, &cursor, &ip->cache[1], ip);
    file_offset = page->index * PAGE_SIZE;

    if (file_offset > inode->i_size) {
        error = -ENOSPC;
        goto done;
    }

    SetPageUptodate (page);
    page_addr = kmap (page);

    if(!page_addr) {
        error = -ENOSPC;
        goto failed;
    }

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
    cursor.key_beg.key = file_offset + 1;
    cursor.asof = ip->obj_asof;
    cursor.flags |= HAMMER_CURSOR_ASOF;

    cursor.key_end = cursor.key_beg;
    KKASSERT(ip->ino_data.obj_type == HAMMER_OBJTYPE_REGFILE);

    cursor.key_beg.rec_type = HAMMER_RECTYPE_DATA;
    cursor.key_end.rec_type = HAMMER_RECTYPE_DATA;
    cursor.key_end.key = 0x7FFFFFFFFFFFFFFFLL;
    cursor.flags |= HAMMER_CURSOR_END_INCLUSIVE;

    error = hammer_ip_first(&cursor);
    boff = 0;

    while(error == 0) {
       /*
        * Get the base file offset of the record.  The key for
        * data records is (base + bytes) rather then (base).
        */
        base = &cursor.leaf->base;
        rec_offset = base->key - cursor.leaf->data_len;

       /*
        * Calculate the gap, if any, and zero-fill it.
        *
        * n is the offset of the start of the record verses our
        * current seek offset in the bio.
        */
        n = (int)(rec_offset - (file_offset + boff));
        if (n > 0) {
            if (n > PAGE_SIZE - boff)
                n = PAGE_SIZE - boff;
            bzero((char *)page_addr + boff, n);
            boff += n;
            n = 0;
        }

       /*
        * Calculate the data offset in the record and the number
        * of bytes we can copy.
        *
        * There are two degenerate cases.  First, boff may already
        * be at bp->b_bufsize.  Secondly, the data offset within
        * the record may exceed the record's size.
        */
        roff = -n;
        rec_offset += roff;
        n = cursor.leaf->data_len - roff;
        if (n <= 0) {
            printk("hammerfs_readpage: bad n=%d roff=%d\n", n, roff);
            n = 0;
        } else if (n > PAGE_SIZE - boff) {
            n = PAGE_SIZE - boff;
        }

       /*
        * Deal with cached truncations.  This cool bit of code
        * allows truncate()/ftruncate() to avoid having to sync
        * the file.
        *
        * If the frontend is truncated then all backend records are
        * subject to the frontend's truncation.
        *
        * If the backend is truncated then backend records on-disk
        * (but not in-memory) are subject to the backend's
        * truncation.  In-memory records owned by the backend
        * represent data written after the truncation point on the
        * backend and must not be truncated.
        *
        * Truncate operations deal with frontend buffer cache
        * buffers and frontend-owned in-memory records synchronously.
        */
       if (ip->flags & HAMMER_INODE_TRUNCATED) {
               if (hammer_cursor_ondisk(&cursor) ||
                   cursor.iprec->flush_state == HAMMER_FST_FLUSH) {
                       if (ip->trunc_off <= rec_offset)
                               n = 0;
                       else if (ip->trunc_off < rec_offset + n)
                               n = (int)(ip->trunc_off - rec_offset);
               }
       }
       if (ip->sync_flags & HAMMER_INODE_TRUNCATED) {
               if (hammer_cursor_ondisk(&cursor)) {
                       if (ip->sync_trunc_off <= rec_offset)
                               n = 0;
                       else if (ip->sync_trunc_off < rec_offset + n)
                               n = (int)(ip->sync_trunc_off - rec_offset);
               }
       }

       /*
        * Calculate the data offset in the record and the number
        * of bytes we can copy.
        */
        disk_offset = cursor.leaf->data_offset + roff;

        // the largest request we can satisfy is PAGE_SIZE
        if(n > PAGE_SIZE) {
            n = PAGE_SIZE;
        }

        while(n > 0) {
            // move this to hammerfs_direct_io_read
            zone2_offset = hammer_blockmap_lookup(hmp, disk_offset, &error);
            vol_no = HAMMER_VOL_DECODE(zone2_offset);
            volume = hammer_get_volume(hmp, vol_no, &error);

            disk_offset = volume->ondisk->vol_buf_beg + (zone2_offset & HAMMER_OFF_SHORT_MASK);

            block_num = disk_offset / BLOCK_SIZE;
            block_offset = disk_offset % BLOCK_SIZE;

           /*
            * Read from disk
            */
            bh = sb_bread(sb, block_num);
            if(!bh) {
                error = -ENOMEM;
                goto failed;
            }
            memcpy((char*)page_addr + roff, (char*)bh->b_data + boff + block_offset, n);
            n -= BLOCK_SIZE;
            brelse(bh);
        }

       /*
        * Iterate until we have filled the request.
        */
        boff += n;
        if (boff == PAGE_SIZE)
            break;
        error = hammer_ip_next(&cursor);
    }

    hammer_done_cursor(&cursor);
    hammer_done_transaction(&trans);

failed:
    if (PageLocked (page))
        unlock_page (page);
    kunmap (page);
done:
    return error;
}

struct address_space_operations hammerfs_address_space_operations = {
    .readpage = hammerfs_readpage
};
