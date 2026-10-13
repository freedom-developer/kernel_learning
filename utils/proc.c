#include "proc.h"

struct proc_file_s {
    struct proc_dir_entry *entry;
    struct proc_ops ops;
    void *data;
    proc_show show;
    proc_write write;
};

static int proc_file_open(struct inode *inode, struct file *filp)
{
    proc_file_t *proc_file = (proc_file_t *)pde_data(inode);
    if (!proc_file->show)
        return -EINVAL;
    return single_open(filp, proc_file->show, proc_file->data);
}

static ssize_t proc_file_write(struct file *file, const char __user *buffer, size_t count, loff_t *offset)
{
    ssize_t ret;
    proc_file_t *proc_file = (proc_file_t *)pde_data(file_inode(file));
    if (!proc_file || !proc_file->write)
        return -EINVAL;
    ret = proc_file->write(buffer, count, proc_file->data);
    if (ret > 0)
        *offset += ret;
    
    return ret;
}

/* 创建一个procfs文件，该文件读写一个单一变量 */
proc_file_t *proc_file_create_rw_parent(const char *name, umode_t mode, struct proc_dir_entry *parent, 
    proc_show show, proc_write write, void *data)
{
    proc_file_t *proc_file;

    proc_file = kzalloc(sizeof(*proc_file), GFP_KERNEL);
    if (!proc_file)
        return ERR_PTR(-ENOMEM);

    proc_file->ops.proc_open = proc_file_open;
    proc_file->ops.proc_read = seq_read;
    proc_file->ops.proc_write = proc_file_write;
    proc_file->ops.proc_lseek = seq_lseek;
    proc_file->ops.proc_release = single_release;
    
    proc_file->data = data;
    proc_file->show = show;
    proc_file->write = write;

    proc_file->entry = proc_create_data(name, mode, parent, &proc_file->ops, (void *)proc_file);
    if (!proc_file->entry) {
        kfree(proc_file);
        return ERR_PTR(-ENOMEM);
    }

    return proc_file;
}




void proc_file_remove(proc_file_t *proc_file)
{
    if (IS_ERR_OR_NULL(proc_file))
        return;
    if (proc_file->entry)
        proc_remove(proc_file->entry);
    kfree(proc_file);
}
