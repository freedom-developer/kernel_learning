#ifndef UTILS_PROC_H
#define UTILS_PROC_H

#include <linux/proc_fs.h>
#include <linux/seq_file.h>

typedef int (*proc_show)(struct seq_file *, void *);
typedef ssize_t (*proc_write)(const char __user *, size_t, void *data);
typedef struct proc_file_s proc_file_t;

/* 创建一个procfs文件，该文件读写一个单一变量 */
proc_file_t *proc_file_create_rw_parent(const char *name, umode_t mode, struct proc_dir_entry *parent, 
    proc_show show, proc_write write, void *data);
#define proc_file_create_rw(name, show, write, data) \
    proc_file_create_rw_parent(name, 0600, NULL, show, write, data)
#define proc_file_create_rw_p(name, parent, show, write, data) \
    proc_file_create_rw_parent(name, 0600, parent, show, write, data)

proc_file_t *proc_file_create_single_data_parent(const char *name, umode_t mode, struct proc_dir_entry *dir,
    proc_show show, void *data);
#define proc_file_create_single_data_p(name, parent, show, data) \
    proc_file_create_single_data_parent(name, 0400, parent, show, data)
#define proc_file_create_single_data(name, show, data) \
    proc_file_create_single_data_p(name, NULL, show, data)
#define proc_file_create_single_p(name, parent,  show) \
    proc_file_create_single_data_p(name, parent, show, NULL)
#define proc_file_create_single(name, show) \
    proc_file_create_single_p(name, NULL, show)


/* 创建一个只读一个数组变量的procfs文件, 带seq_operations */
proc_file_t *proc_file_create_parent_seq_ops(const char *name, umode_t mode, struct proc_dir_entry *parent, 
    struct seq_operations *seq_ops, void *data);
#define proc_file_create_seq_ops_p(name, parent, seq_ops, data) \
    proc_file_create_parent_seq_ops(name, 0400, parent, seq_ops, data)
#define proc_file_create_seq_ops(name, seq_ops, data) \
    proc_file_create_seq_ops_p(name, NULL, seq_ops, data)

/* 创建一个只读一个数组变量的procfs文件: 对于数组元素连接保存时，可以包装seq_operations */
proc_file_t *proc_file_create_array_parent(const char *name, umode_t mode, struct proc_dir_entry *parent, 
    proc_show show, void *data, size_t element_size, size_t element_count);
#define proc_file_create_array_p(name, parent, show, data, esize, ecount) \
    proc_file_create_array_parent(name, 0400, parent, show, data, esize, ecount)
#define proc_file_create_array(name, show, data, esize, ecount) \
    proc_file_create_array_p(name, NULL, show, data, esize, ecount)


void proc_file_remove(proc_file_t *proc_file);

#endif
