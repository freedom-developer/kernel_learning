#include "demo_fs.h"

#include <linux/fs.h>
#include <linux/fs_context.h>

#define DEMOFS_MAGIC 0x18189182

static int demofs_fill_super(struct super_block *sb, struct fs_context *fc)
{
    static const struct tree_descr files [] = {
        {""}
    };

    return simple_fill_super(sb, DEMOFS_MAGIC, files);
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