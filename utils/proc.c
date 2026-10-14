#include "proc.h"
#include <linux/overflow.h>

struct proc_file_s {
    struct proc_dir_entry *entry;

    void *data;
    size_t element_size;
    size_t element_count;
    proc_show show;
    proc_write write;
};

static int proc_file_rw_open(struct inode *inode, struct file *filp)
{
    proc_file_t *proc_file = (proc_file_t *)pde_data(inode);
    if (!proc_file || !proc_file->show)
        return -EINVAL;
    return single_open(filp, proc_file->show, proc_file->data);
}

static ssize_t proc_file_rw_write(struct file *file, const char __user *buffer, size_t count, loff_t *offset)
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

static const struct proc_ops proc_file_rw_ops = {
    .proc_open = proc_file_rw_open,
    .proc_read = seq_read,
    .proc_write = proc_file_rw_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

/* 创建一个procfs文件，该文件读写一个单一变量 */
proc_file_t *proc_file_create_rw_parent(const char *name, umode_t mode, struct proc_dir_entry *parent, 
    proc_show show, proc_write write, void *data)
{
    proc_file_t *proc_file;

    if (!name || !show || !write)
	    return ERR_PTR(-EINVAL);

    proc_file = kzalloc(sizeof(*proc_file), GFP_KERNEL);
    if (!proc_file)
        return ERR_PTR(-ENOMEM);
    
    proc_file->data = data;
    proc_file->show = show;
    proc_file->write = write;

    proc_file->entry = proc_create_data(name, mode, parent, &proc_file_rw_ops, (void *)proc_file);
    if (!proc_file->entry) {
        kfree(proc_file);
        return ERR_PTR(-ENOMEM);
    }

    return proc_file;
}

/* 创建一个只读一个变量的procfs文件 */
proc_file_t *proc_file_create_single_data_parent(const char *name, umode_t mode, struct proc_dir_entry *dir,
    proc_show show, void *data)
{
    proc_file_t *proc_file;
    if (!name || !show)
	    return ERR_PTR(-EINVAL);

    proc_file = kzalloc(sizeof(*proc_file), GFP_KERNEL);
    if (!proc_file)
        return ERR_PTR(-ENOMEM);
    proc_file->entry = proc_create_single_data(name, mode, dir, show, data);
    if (!proc_file->entry) {
        kfree(proc_file);
        return ERR_PTR(-ENOMEM);
    }
    return proc_file;
}

/* 创建一个只读一个数组变量的procfs文件, 带seq_operations */
proc_file_t *proc_file_create_parent_seq_ops(const char *name, umode_t mode, struct proc_dir_entry *parent, 
    struct seq_operations *seq_ops, void *data)
{
    proc_file_t *proc_file;

    if (!name || !seq_ops ||
    !seq_ops->start || !seq_ops->next ||
    !seq_ops->stop || !seq_ops->show)
	    return ERR_PTR(-EINVAL);

    proc_file = kzalloc(sizeof(*proc_file), GFP_KERNEL);
    if (!proc_file)
        return ERR_PTR(-ENOMEM);
    proc_file->entry = proc_create_seq_data(name, mode, parent, seq_ops, data);
    if (!proc_file->entry) {
        kfree(proc_file);
        return ERR_PTR(-ENOMEM);
    }

    return proc_file;
}

static void *proc_file_array_start(struct seq_file *seq, loff_t *pos)
{
    proc_file_t *proc_file;

    proc_file = pde_data(file_inode(seq->file));
    if (*pos < 0 || (u64)*pos >= proc_file->element_count)
        return NULL;
    return (char *) proc_file->data + (size_t)*pos * proc_file->element_size;
}

static void *proc_file_array_next(struct seq_file *seq, void *element, loff_t *pos)
{
    proc_file_t *proc_file;

    proc_file = pde_data(file_inode(seq->file));
    ++*pos;
    if ((u64)*pos >= proc_file->element_count)
        return NULL;
    return (char *) proc_file->data + (size_t)*pos * proc_file->element_size;
}

static void proc_file_array_stop(struct seq_file *seq, void *element)
{
}

static int proc_file_array_show(struct seq_file *seq, void *element)
{
    proc_file_t *proc_file;

    proc_file = pde_data(file_inode(seq->file));
    return proc_file->show(seq, element);
}

static const struct seq_operations proc_file_array_seq_ops = {
    .start = proc_file_array_start,
    .next = proc_file_array_next,
    .stop = proc_file_array_stop,
    .show = proc_file_array_show,
};


/* 创建一个只读一个数组变量的procfs文件: 对于数组元素连接保存时，可以包装seq_operations */
proc_file_t *proc_file_create_array_parent(const char *name, umode_t mode, struct proc_dir_entry *parent, 
    proc_show show, void *data, size_t element_size, size_t element_count)
{
    proc_file_t *proc_file;

    if (!name || !show || !element_size)
	    return ERR_PTR(-EINVAL);
    if (element_count && !data)
	    return ERR_PTR(-EINVAL);

    size_t total_size;
    if (check_mul_overflow(element_count, element_size, &total_size))
	    return ERR_PTR(-EOVERFLOW);

    proc_file = kzalloc(sizeof(*proc_file), GFP_KERNEL);
    if (!proc_file)
        return ERR_PTR(-ENOMEM);

    proc_file->element_count = element_count;
    proc_file->element_size = element_size;
    proc_file->data = data;
    proc_file->show = show;

    proc_file->entry = proc_create_seq_data(name, mode, parent, &proc_file_array_seq_ops, proc_file);
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
