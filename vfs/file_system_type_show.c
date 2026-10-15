// SPDX-License-Identifier: GPL-2.0
#include "file_system_type_show.h"

#include <linux/fs.h>
#include <linux/fs_parser.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>

static proc_file_t *proc_file_fs_type;

/*
 * file_systems 和 file_systems_lock 在 fs/filesystems.c 中是 static 符号，
 * 因此模块不能直接引用它们。地址必须由加载模块的当前内核提供；不能把
 * 某一次启动时的地址硬编码进模块（开启 KASLR 时每次启动都可能不同）。
 */
static unsigned long file_systems_addr;
module_param(file_systems_addr, ulong, 0400);
MODULE_PARM_DESC(file_systems_addr,
		 "address of the kernel's file_systems variable");

static unsigned long file_systems_lock_addr;
module_param(file_systems_lock_addr, ulong, 0400);
MODULE_PARM_DESC(file_systems_lock_addr,
		 "address of the kernel's file_systems_lock");

struct fs_type_flag_name {
	unsigned int mask;
	const char *name;
};

/* 这些标志描述文件系统驱动的能力，而非某次挂载的选项。 */
static const struct fs_type_flag_name fs_type_flags[] = {
	{ FS_REQUIRES_DEV, "requires_dev" },
	{ FS_BINARY_MOUNTDATA, "binary_mountdata" },
	{ FS_HAS_SUBTYPE, "has_subtype" },
	{ FS_USERNS_MOUNT, "userns_mount" },
	{ FS_DISALLOW_NOTIFY_PERM, "disallow_notify_perm" },
	{ FS_ALLOW_IDMAP, "allow_idmap" },
	{ FS_RENAME_DOES_D_MOVE, "rename_does_d_move" },
};

static void fs_type_print_flags(struct seq_file *seq, unsigned int flags)
{
	bool first = true;
	unsigned int i;

	seq_printf(seq, "  fs_flags: %#x [", flags);
	for (i = 0; i < ARRAY_SIZE(fs_type_flags); ++i) {
		if (!(flags & fs_type_flags[i].mask))
			continue;
		seq_printf(seq, "%s%s", first ? "" : ",",
			   fs_type_flags[i].name);
		first = false;
	}
	seq_puts(seq, "]\n");
}

static void fs_type_print_parameters(struct seq_file *seq,
				     const struct fs_parameter_spec *spec)
{
	unsigned int index = 0;

	if (!spec) {
		seq_puts(seq, "  parameters: none\n");
		return;
	}

	/* parameters 是支持的挂载参数描述表，并不是当前挂载的参数值。 */
	seq_puts(seq, "  parameters:\n");
	for (; spec->name; ++spec, ++index) {
		seq_printf(seq, "    [%u] name=%s type=", index, spec->name);
		/* type 为空表示不带值的开关参数，否则是参数类型解析函数。 */
		if (spec->type)
			seq_printf(seq, "%ps", (void *)spec->type);
		else
			seq_puts(seq, "flag");
		/* data 的含义由参数类型决定，不能按通用类型解引用。 */
		seq_printf(seq, " opt=%u flags=%#x data=%p\n",
			   spec->opt, spec->flags, spec->data);
		if (seq_has_overflowed(seq))
			return;
	}
}

static int fs_type_show(struct seq_file *seq, void *unused)
{
	struct file_system_type **file_systems;
	rwlock_t *file_systems_lock;
	struct file_system_type *fs_type;
	unsigned int index = 0;

	/* kallsyms 中的 file_systems 是头指针变量的地址，需再解引用一次。 */
	file_systems = (struct file_system_type **)file_systems_addr;
	file_systems_lock = (rwlock_t *)file_systems_lock_addr;

	/* 注册和注销文件系统都受此锁保护；不能无锁遍历该链表。 */
	read_lock(file_systems_lock);
	for (fs_type = READ_ONCE(*file_systems); fs_type;
	     fs_type = READ_ONCE(fs_type->next), ++index) {
		seq_printf(seq, "filesystem[%u] @%p:\n", index, fs_type);
		seq_printf(seq, "  name: %s\n", fs_type->name ?: "(null)");
		fs_type_print_flags(seq, (unsigned int)fs_type->fs_flags);
		/* 内建文件系统的 owner 为空；module_name() 将其显示为 kernel。 */
		seq_printf(seq, "  owner: %s\n", module_name(fs_type->owner));
		/* 新接口初始化挂载上下文；mount 则是旧式挂载接口。 */
		seq_printf(seq, "  init_fs_context: %ps\n",
			   (void *)fs_type->init_fs_context);
	}
	read_unlock(file_systems_lock);

	return 0;
}

int file_system_type_show_init(struct proc_dir_entry *dir)
{
	int ret;

	if (!file_systems_addr || !file_systems_lock_addr) {
		pr_err("vfs_demo: both address module parameters are required\n");
		return -EINVAL;
	}

	proc_file_fs_type = proc_file_create_single_p("fs_types", dir, fs_type_show);
	if (IS_ERR(proc_file_fs_type)) {
		ret = PTR_ERR(proc_file_fs_type);
		proc_file_fs_type = NULL;
		return ret;
	}

	return 0;
}

void file_system_type_show_exit(void)
{
	if (proc_file_fs_type)
		proc_file_remove(proc_file_fs_type);
	proc_file_fs_type = NULL;
}
