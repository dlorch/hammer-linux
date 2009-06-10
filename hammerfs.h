#ifndef _HAMMERFS_H
#define _HAMMERFS_H

#include <linux/fs.h>

#include "dfly_wrap.h"

#include "hammer.h"

extern struct inode_operations hammerfs_inode_operations;
extern struct file_operations hammerfs_file_operations;
extern struct file_system_type hammerfs_type;
extern struct super_operations hammerfs_super_operations;
extern struct address_space_operations hammerfs_address_space_operations;

int hammerfs_get_inode(struct super_block *sb, struct hammer_inode *ip, struct inode **inode);
int hammerfs_get_itype(char obj_type);

#endif /* _HAMMERFS_H */
