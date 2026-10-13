// SPDX-License-Identifier: GPL-2.0
#include <linux/compiler.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kstrtox.h>
#include <linux/module.h>
#include <linux/seq_file.h>

#include "../utils/proc.h"

#define PROC_NAME "demo"

static proc_file_t *demo_proc_file;
static int demo_value;

static int demo_show(struct seq_file *seq, void *unused)
{
	int *value = seq->private;

	seq_printf(seq, "%d\n", READ_ONCE(*value));
	return 0;
}

static ssize_t demo_write(const char __user *buffer, size_t count, void *data)
{
	int *value = data;
	int new_value;
	int ret;

	ret = kstrtoint_from_user(buffer, count, 0, &new_value);
	if (ret)
		return ret;

	WRITE_ONCE(*value, new_value);
	return count;
}

static int __init demo_init(void)
{
	demo_proc_file = proc_file_create_rw(PROC_NAME, demo_show, demo_write, &demo_value);
	if (IS_ERR(demo_proc_file)) {
		pr_err("demo: failed to create /proc/%s\n", PROC_NAME);
		return PTR_ERR(demo_proc_file);
	}

	pr_info("demo: created /proc/%s\n", PROC_NAME);
	return 0;
}

static void __exit demo_exit(void)
{
	proc_file_remove(demo_proc_file);
	pr_info("demo: removed /proc/%s\n", PROC_NAME);
}

module_init(demo_init);
module_exit(demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("kernel_learning");
MODULE_DESCRIPTION("Test the proc utility with a readable and writable integer");
