// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>

#include "../utils/utils.h"
#include "super_blocks_show.h"
#include "file_system_type_show.h"

static struct proc_dir_entry *proc_dir_vfs;

static int __init vfs_demo_init(void)
{
	int ret;

	proc_dir_vfs = proc_mkdir("vfs", NULL);
	if (!proc_dir_vfs)
		return -ENOMEM;

	ret = super_blocks_show_init(proc_dir_vfs);
	if (ret)
		goto remove_proc_dir;

	ret = file_system_type_show_init(proc_dir_vfs);
	if (ret)
		goto remove_supers;

	return 0;

remove_supers:
	super_blocks_show_exit();
remove_proc_dir:
	proc_remove(proc_dir_vfs);
	proc_dir_vfs = NULL;
	return ret;
}

static void __exit vfs_demo_exit(void)
{
	file_system_type_show_exit();
	super_blocks_show_exit();

	if (proc_dir_vfs) {
		proc_remove(proc_dir_vfs);
		proc_dir_vfs = NULL;
	}
}

module_init(vfs_demo_init);
module_exit(vfs_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("kernel_learning");
MODULE_DESCRIPTION("VFS learning demo module");
