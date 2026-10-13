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
#define proc_file_create_rw(name, show, write, data) proc_file_create_rw_parent(name, 0600, NULL, show, write, data)

void proc_file_remove(proc_file_t *proc_file);

#endif
