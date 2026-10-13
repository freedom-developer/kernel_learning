// SPDX-License-Identifier: GPL-2.0
#include <linux/compiler.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kstrtox.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "../utils/proc.h"

#define DEMO_DIR_NAME "proc_demo"

struct demo_status {
	const char *name;
	unsigned int version;
};

struct demo_item {
	unsigned int id;
	const char *name;
	long value;
};

struct demo_table {
	struct demo_item *items;
	size_t count;
};

enum demo_proc_file_id {
	DEMO_PROC_RW_VALUE,
	DEMO_PROC_SINGLE_DATA,
	DEMO_PROC_SINGLE,
	DEMO_PROC_CUSTOM_SEQ,
	DEMO_PROC_ARRAY,
	DEMO_PROC_FILE_COUNT,
};

static struct proc_dir_entry *demo_dir;
static proc_file_t *demo_proc_files[DEMO_PROC_FILE_COUNT];

static int demo_value = 100;
static struct demo_status demo_status = {
	.name = "proc utility demo",
	.version = 1,
};
static struct demo_item demo_items[] = {
	{ .id = 1, .name = "alpha", .value = 10 },
	{ .id = 2, .name = "beta", .value = 20 },
	{ .id = 3, .name = "gamma", .value = 30 },
};
static struct demo_table demo_table = {
	.items = demo_items,
	.count = ARRAY_SIZE(demo_items),
};

static int demo_value_show(struct seq_file *seq, void *unused)
{
	int *value = seq->private;

	seq_printf(seq, "%d\n", READ_ONCE(*value));
	return 0;
}

static ssize_t demo_value_write(const char __user *buffer, size_t count,
				void *data)
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

static int demo_status_show(struct seq_file *seq, void *unused)
{
	struct demo_status *status = seq->private;

	seq_printf(seq, "name=%s version=%u\n", status->name, status->version);
	return 0;
}

static int demo_single_show(struct seq_file *seq, void *unused)
{
	seq_puts(seq, "single callback without private data\n");
	return 0;
}

/*
 * proc_create_seq_data() keeps its data in the proc inode. Unlike
 * single_open(), it does not pass that pointer to show() as seq->private.
 */
static struct demo_table *demo_seq_table(struct seq_file *seq)
{
	return pde_data(file_inode(seq->file));
}

static void *demo_seq_start(struct seq_file *seq, loff_t *pos)
{
	struct demo_table *table = demo_seq_table(seq);

	if (*pos < 0 || (u64)*pos >= table->count)
		return NULL;
	return &table->items[*pos];
}

static void *demo_seq_next(struct seq_file *seq, void *item, loff_t *pos)
{
	struct demo_table *table = demo_seq_table(seq);

	++*pos;
	if ((u64)*pos >= table->count)
		return NULL;
	return &table->items[*pos];
}

static void demo_seq_stop(struct seq_file *seq, void *item)
{
}

static int demo_item_show(struct seq_file *seq, void *item)
{
	struct demo_item *demo_item = item;

	seq_printf(seq, "id=%u name=%s value=%ld\n", demo_item->id,
		   demo_item->name, demo_item->value);
	return 0;
}

static struct seq_operations demo_seq_ops = {
	.start = demo_seq_start,
	.next = demo_seq_next,
	.stop = demo_seq_stop,
	.show = demo_item_show,
};

static int demo_save_proc_file(enum demo_proc_file_id id, const char *name,
			       proc_file_t *proc_file)
{
	int ret;

	if (!IS_ERR_OR_NULL(proc_file)) {
		demo_proc_files[id] = proc_file;
		return 0;
	}

	ret = proc_file ? PTR_ERR(proc_file) : -ENOMEM;
	pr_err("demo: failed to create /proc/%s/%s: %d\n",
	       DEMO_DIR_NAME, name, ret);
	return ret;
}

static void demo_remove_proc_files(void)
{
	int i;

	for (i = DEMO_PROC_FILE_COUNT - 1; i >= 0; --i) {
		proc_file_remove(demo_proc_files[i]);
		demo_proc_files[i] = NULL;
	}
}

static int __init demo_init(void)
{
	proc_file_t *proc_file;
	int ret;

	demo_dir = proc_mkdir(DEMO_DIR_NAME, NULL);
	if (!demo_dir)
		return -ENOMEM;

	proc_file = proc_file_create_rw_parent("rw_value", 0600, demo_dir,
					       demo_value_show, demo_value_write,
					       &demo_value);
	ret = demo_save_proc_file(DEMO_PROC_RW_VALUE, "rw_value", proc_file);
	if (ret)
		goto err_remove;

	proc_file = proc_file_create_single_data_parent("single_data", 0400,
						   demo_dir, demo_status_show,
						   &demo_status);
	ret = demo_save_proc_file(DEMO_PROC_SINGLE_DATA, "single_data",
				  proc_file);
	if (ret)
		goto err_remove;

	proc_file = proc_file_create_single_p("single", demo_dir,
					     demo_single_show);
	ret = demo_save_proc_file(DEMO_PROC_SINGLE, "single", proc_file);
	if (ret)
		goto err_remove;

	proc_file = proc_file_create_parent_seq_ops("custom_seq", 0400,
						      demo_dir, &demo_seq_ops,
						      &demo_table);
	ret = demo_save_proc_file(DEMO_PROC_CUSTOM_SEQ, "custom_seq", proc_file);
	if (ret)
		goto err_remove;

	proc_file = proc_file_create_array_parent("array", 0400, demo_dir,
						 demo_item_show, demo_items,
						 sizeof(demo_items[0]),
						 ARRAY_SIZE(demo_items));
	ret = demo_save_proc_file(DEMO_PROC_ARRAY, "array", proc_file);
	if (ret)
		goto err_remove;

	pr_info("demo: created procfs tests under /proc/%s\n", DEMO_DIR_NAME);
	return 0;

err_remove:
	demo_remove_proc_files();
	proc_remove(demo_dir);
	demo_dir = NULL;
	return ret;
}

static void __exit demo_exit(void)
{
	demo_remove_proc_files();
	proc_remove(demo_dir);
	demo_dir = NULL;
	pr_info("demo: removed /proc/%s\n", DEMO_DIR_NAME);
}

module_init(demo_init);
module_exit(demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("kernel_learning");
MODULE_DESCRIPTION("Test every proc utility interface");
