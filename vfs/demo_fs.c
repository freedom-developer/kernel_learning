#include "demo_fs.h"

#include <linux/fs.h>
#include <linux/fs_context.h>
#include <linux/pagemap.h>

#define DEMOFS_MAGIC 0x18189182

struct inode *demofs_get_inode(struct super_block *sb, const struct inode *dir, umode_t mode, dev_t dev);

static int demofs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode, dev_t dev)
{
    struct inode *inode;
    inode = demofs_get_inode(dir->i_sb, dir, mode, dev);
    if (!inode)
        return -ENOSPC;
    d_instantiate(dentry, inode);
    
    /* 将dentry保留在dcache中。
     * simple_unlink()删除文件时会对应地调用dput()
     */
    dget(dentry);
    inode_set_mtime_to_ts(dir, inode_set_ctime_current(dir));
    return 0;
}


static int demofs_create(struct mnt_idmap *idmap, struct inode *dir, struct dentry *dentry, umode_t mode, bool excl)
{
    return demofs_mknod(dir, dentry, mode | S_IFREG, 0);
}

static int demofs_mkdir(struct mnt_idmap *idmap, struct inode *dir, struct dentry *dentry, umode_t mode)
{
    int ret;
    ret = demofs_mknod(dir, dentry, mode | S_IFDIR, 0);
    if (!ret)
        inc_nlink(dir);
    return ret;
}

static const struct inode_operations demofs_dir_inode_operations = {
    .lookup = simple_lookup,
    .create = demofs_create,
    .mkdir = demofs_mkdir,
    .unlink = simple_unlink,
    .rmdir = simple_rmdir,
    .rename = simple_rename,
    .link = simple_link,
};

static const struct file_operations demofs_file_operations = {
    .owner = THIS_MODULE,
    .read_iter = generic_file_read_iter,
    .write_iter = generic_file_write_iter,
    .mmap = generic_file_mmap,
    .fsync = noop_fsync,
    .splice_read = filemap_splice_read,
    .splice_write = iter_file_splice_write,
    .llseek = generic_file_llseek,
};

static const struct inode_operations demofs_file_inode_operations = {
    .setattr = simple_setattr,
    .getattr = simple_getattr,
};

struct inode *demofs_get_inode(struct super_block *sb, const struct inode *dir, umode_t mode, dev_t dev)
{
    struct inode *inode;

    inode = new_inode(sb);
    if (!inode)
        return NULL;
    inode->i_ino = get_next_ino();
    inode_init_owner(&nop_mnt_idmap, inode, dir, mode);
    simple_inode_init_ts(inode);

    /* 普通文件的数据存放在页缓存中
     * ram_aops提供read_folio, write_begin, write_end等操作
     */
    inode->i_mapping->a_ops = &ram_aops;
    mapping_set_gfp_mask(inode->i_mapping, GFP_HIGHUSER);
    mapping_set_unevictable(inode->i_mapping);

    switch (mode & S_IFMT) {
    case S_IFREG:
        inode->i_op = &demofs_file_inode_operations;
        inode->i_fop = &demofs_file_operations;
        break;
    case S_IFDIR:
        inode->i_op = &demofs_dir_inode_operations;
        inode->i_fop = &simple_dir_operations;
        inc_nlink(inode);
        break;
    default:
        init_special_inode(inode, mode, dev);
        break;
    }


    return inode;
}

static const struct super_operations demofs_super_operations = {
    .statfs = simple_statfs,
    .drop_inode = generic_delete_inode,
};

static int demofs_fill_super(struct super_block *sb, struct fs_context *fc)
{
    struct inode *root_inode;

    sb->s_maxbytes = MAX_LFS_FILESIZE;
    sb->s_blocksize = PAGE_SIZE;
    sb->s_blocksize_bits = PAGE_SHIFT;
    sb->s_magic = DEMOFS_MAGIC;
    sb->s_op = &demofs_super_operations;
    sb->s_time_gran = 1;
    
    root_inode = demofs_get_inode(sb, NULL, S_IFDIR | 0755, 0);
    if (!root_inode)
        return -ENOMEM;
    sb->s_root = d_make_root(root_inode);
    if (!sb->s_root)
        return -ENOMEM;
    return 0;
}

static int demofs_get_tree(struct fs_context *fc)
{
    return get_tree_nodev(fc, demofs_fill_super);
}

static struct fs_context_operations demofs_context_ops = {
    .get_tree = demofs_get_tree,
};

static int demofs_init_fs_context(struct fs_context *fc)
{
    fc->ops = &demofs_context_ops;

    return 0;
}


static struct file_system_type demofs_type = {
    .owner = THIS_MODULE,
    .name = "demofs",
    .init_fs_context = demofs_init_fs_context,
    .kill_sb = kill_litter_super,
    
    .fs_flags = 0,
};

int demo_fs_init(void)
{
    return register_filesystem(&demofs_type);
}

void demo_fs_exit(void)
{
    unregister_filesystem(&demofs_type);
}