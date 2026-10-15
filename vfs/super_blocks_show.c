// SPDX-License-Identifier: GPL-2.0
#include <linux/backing-dev.h>
#include <linux/blkdev.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "../utils/utils.h"
#include "super_blocks_show.h"

struct flag_name {
	unsigned long mask;
	const char *name;
};

struct supers_context {
	struct seq_file *seq;
	/* 本次读取期间已经输出的 superblock 数量，并非稳定的系统 ID。 */
	unsigned int count;
};

/*
 * sb->s_flags 中既有文件系统行为标志，也有 VFS 生命周期标志。
 * 注意：这些是 superblock 级标志；MS_NOSUID 等挂载点级标志还可能保存
 * 在 struct mount 中，因此这里不能完整替代 /proc/self/mountinfo。
 */
static const struct flag_name superblock_flags[] = {
	{ SB_RDONLY, "ro" },             /* 整个文件系统只读。 */
	{ SB_NOSUID, "nosuid" },         /* 忽略 setuid/setgid 位。 */
	{ SB_NODEV, "nodev" },           /* 禁止访问设备特殊文件。 */
	{ SB_NOEXEC, "noexec" },         /* 禁止直接执行文件。 */
	{ SB_SYNCHRONOUS, "sync" },       /* 写操作采用同步方式。 */
	{ SB_MANDLOCK, "mandlock" },      /* 允许强制文件锁。 */
	{ SB_DIRSYNC, "dirsync" },        /* 目录修改采用同步方式。 */
	{ SB_NOATIME, "noatime" },        /* 不更新文件访问时间。 */
	{ SB_NODIRATIME, "nodiratime" },  /* 不更新目录访问时间。 */
	{ SB_SILENT, "silent" },          /* 抑制部分文件系统提示信息。 */
	{ SB_POSIXACL, "posixacl" },      /* 文件系统支持 POSIX ACL。 */
	{ SB_INLINECRYPT, "inlinecrypt" }, /* 使用块层内联加密。 */
	{ SB_KERNMOUNT, "kernmount" },    /* 由内核内部发起的挂载。 */
	{ SB_I_VERSION, "iversion" },     /* 更新 inode 的版本计数。 */
	{ SB_LAZYTIME, "lazytime" },      /* 延迟把 inode 时间写入存储介质。 */
	{ SB_DEAD, "dead" },              /* superblock 已完成关闭。 */
	{ SB_DYING, "dying" },            /* superblock 正在关闭。 */
	{ SB_SUBMOUNT, "submount" },      /* 由另一个挂载自动产生的子挂载。 */
	{ SB_FORCE, "force" },            /* 本次操作要求强制处理。 */
	{ SB_NOSEC, "nosec" },            /* 可跳过部分 inode 安全属性处理。 */
	{ SB_BORN, "born" },              /* superblock 已完成初始化并可见。 */
	{ SB_ACTIVE, "active" },          /* superblock 当前处于活动状态。 */
	{ SB_NOUSER, "nouser" },          /* 内部伪文件系统，不供用户挂载。 */
};

/* sb->s_iflags 是只供 VFS/文件系统内部使用的 per-superblock 标志。 */
static const struct flag_name superblock_internal_flags[] = {
	{ SB_I_CGROUPWB, "cgroupwb" }, /* 支持按 cgroup 跟踪回写。 */
	{ SB_I_NOEXEC, "noexec" },     /* VFS 内部强制 noexec。 */
	{ SB_I_NODEV, "nodev" },       /* VFS 内部强制 nodev。 */
	/* 回写完成前，不能再次修改相应的数据块。 */
	{ SB_I_STABLE_WRITES, "stable_writes" },
	/* 允许该类型的已有实例对用户命名空间可见。 */
	{ SB_I_USERNS_VISIBLE, "userns_visible" },
	/* IMA 无法验证该文件系统上的文件签名。 */
	{ SB_I_IMA_UNVERIFIABLE_SIGNATURE, "ima_unverifiable_signature" },
	/* 该 superblock 由不受信任的用户命名空间挂载者创建。 */
	{ SB_I_UNTRUSTED_MOUNTER, "untrusted_mounter" },
	/* 该文件系统不支持 EVM。 */
	{ SB_I_EVM_UNSUPPORTED, "evm_unsupported" },
	{ SB_I_SKIP_SYNC, "skip_sync" }, /* 全局 sync 操作跳过此 superblock。 */
	{ SB_I_PERSB_BDI, "persb_bdi" }, /* 拥有独立的 backing_dev_info。 */
	/* 已经报告过时间戳有效范围即将到期的警告。 */
	{ SB_I_TS_EXPIRY_WARNED, "ts_expiry_warned" },
	{ SB_I_RETIRED, "retired" }, /* 已退役，不应再复用。 */
	{ SB_I_NOUMASK, "noumask" }, /* VFS 创建 inode 时不应用 umask。 */
};

/* sb->s_type->fs_flags 描述文件系统驱动本身的能力，不是挂载选项。 */
static const struct flag_name filesystem_type_flags[] = {
	{ FS_REQUIRES_DEV, "requires_dev" }, /* 挂载时需要块设备。 */
	{ FS_BINARY_MOUNTDATA, "binary_mountdata" }, /* 使用二进制挂载数据。 */
	{ FS_HAS_SUBTYPE, "has_subtype" }, /* 支持 type.subtype 形式。 */
	{ FS_USERNS_MOUNT, "userns_mount" }, /* 用户命名空间 root 可挂载。 */
	/* 禁止该文件系统产生 fanotify 权限事件。 */
	{ FS_DISALLOW_NOTIFY_PERM, "disallow_notify_perm" },
	{ FS_ALLOW_IDMAP, "allow_idmap" }, /* 支持 idmapped mount。 */
	/* rename 方法会自行完成 d_move()。 */
	{ FS_RENAME_DOES_D_MOVE, "rename_does_d_move" },
};

typedef void (*iterate_super_fn)(
	void (*callback)(struct super_block *, void *), void *arg);

static proc_file_t *proc_file_supers;


static void print_flags(struct seq_file *seq, const char *label,
			unsigned long flags, const struct flag_name *names,
			size_t nr_names)
{
	size_t i;
	bool first = true;

	/* 十六进制保留全部位；方括号只翻译上表中当前内核已知的位。 */
	seq_printf(seq, "  %s: %#lx [", label, flags);
	for (i = 0; i < nr_names; ++i) {
		if (!(flags & names[i].mask))
			continue;
		seq_printf(seq, "%s%s", first ? "" : ",", names[i].name);
		first = false;
	}
	seq_puts(seq, "]\n");
}

static const char *freeze_state_name(unsigned short state)
{
	switch (state) {
	case SB_UNFROZEN:
		/* 正常状态，写入不受冻结机制阻塞。 */
		return "unfrozen";
	case SB_FREEZE_WRITE:
		/* 第一阶段：阻止新的普通写入，并等待已有写入完成。 */
		return "freezing-writes";
	case SB_FREEZE_PAGEFAULT:
		/* 第二阶段：进一步阻止可能写脏页面的 page fault。 */
		return "freezing-pagefaults";
	case SB_FREEZE_FS:
		/* 第三阶段：文件系统正在停止内部修改和事务。 */
		return "freezing-filesystem";
	case SB_FREEZE_COMPLETE:
		/* 冻结已经完成。 */
		return "frozen";
	default:
		return "unknown";
	}
}

static void print_super(struct super_block *sb, void *arg)
{
	struct supers_context *ctx = arg;
	struct inode *root_inode = sb->s_root ? d_inode(sb->s_root) : NULL;
	struct name_snapshot root_name;

	/* 输出序号只表示本次遍历顺序；superblock 链表变化后顺序也可能变化。 */
	seq_printf(ctx->seq, "superblock[%u]:\n", ctx->count++);

	/*
	 * filesystem：注册的 file_system_type 名称；如果 s_subtype 非空，
	 * 按照 /proc/mounts 的规则显示为 "type.subtype"，如 fuse.sshfs。
	 */
	seq_printf(ctx->seq, "  filesystem: %s%s%s\n",
		   sb->s_type->name, sb->s_subtype ? "." : "",
		   sb->s_subtype ?: "");

	/* s_id 是文件系统提供的说明性名称，常见值是设备名，但不保证唯一。 */
	seq_printf(ctx->seq, "  id: %s\n", sb->s_id);

	/*
	 * s_dev 是 VFS 用于标识 superblock 的设备号。伪文件系统也可能拥有
	 * 动态分配的设备号，所以非 0 并不意味着一定存在真实块设备。
	 */
	seq_printf(ctx->seq, "  device: %u:%u\n",
		   MAJOR(sb->s_dev), MINOR(sb->s_dev));

	/* 文件系统 UUID；未设置 UUID 的文件系统通常显示为全 0。 */
	seq_printf(ctx->seq, "  uuid: %pUB\n", &sb->s_uuid);

	/* 文件系统 magic，通常也会由 statfs(2) 作为 f_type 返回。 */
	seq_printf(ctx->seq, "  magic: %#lx\n", sb->s_magic);

	/* file_system_type 的拥有者；内建文件系统显示 kernel，模块则显示模块名。 */
	seq_printf(ctx->seq, "  owner: %s\n", module_name(sb->s_type->owner));

	/* 文件系统驱动能力、superblock 标志和 VFS 内部标志，含原始值及名称。 */
	print_flags(ctx->seq, "fs_type_flags", sb->s_type->fs_flags,
		    filesystem_type_flags, ARRAY_SIZE(filesystem_type_flags));
	print_flags(ctx->seq, "flags", sb->s_flags, superblock_flags,
		    ARRAY_SIZE(superblock_flags));
	print_flags(ctx->seq, "internal_flags", sb->s_iflags,
		    superblock_internal_flags,
		    ARRAY_SIZE(superblock_internal_flags));

	/*
	 * 文件系统的逻辑块大小以及 log2(块大小)。正常情况下二者满足
	 * s_blocksize == 1UL << s_blocksize_bits。
	 */
	seq_printf(ctx->seq, "  block_size: %lu bytes (%u bits)\n",
		   sb->s_blocksize, (unsigned int)sb->s_blocksize_bits);

	/* 单个普通文件允许的最大字节数，并非整个文件系统的总容量。 */
	seq_printf(ctx->seq, "  max_file_size: %lld bytes\n",
		   (long long)sb->s_maxbytes);

	/* inode atime/mtime/ctime 可以表达的最小时间粒度，单位为纳秒。 */
	seq_printf(ctx->seq, "  time_granularity: %u ns\n", sb->s_time_gran);

	/* inode 时间戳能够表示的最早/最晚时间，单位为 Unix Epoch 秒。 */
	seq_printf(ctx->seq, "  time_range: [%lld, %lld] seconds\n",
		   (long long)sb->s_time_min, (long long)sb->s_time_max);

	/* inode 硬链接计数上限；值为 0 表示 VFS 不额外施加此项上限。 */
	seq_printf(ctx->seq, "  max_links: %u\n", sb->s_max_links);

	/* 堆叠文件系统深度；普通底层文件系统通常为 0，overlayfs 等可能更大。 */
	seq_printf(ctx->seq, "  stack_depth: %d\n", sb->s_stack_depth);

	/* 当前文件系统冻结阶段；iterate_supers() 持有 s_umount 读锁，可安全读取。 */
	seq_printf(ctx->seq, "  freeze_state: %s (%u)\n",
		   freeze_state_name(sb->s_writers.frozen),
		   (unsigned int)sb->s_writers.frozen);

	/*
	 * s_active 是保持文件系统处于活动状态的内部引用计数，不等于挂载点数，
	 * 也不等于打开文件数；这里只是读取瞬时值。
	 */
	seq_printf(ctx->seq, "  active_refs: %d\n", atomic_read(&sb->s_active));

	/* backing_dev_info 的名字，用于标识页面缓存脏页的回写目标。 */
	seq_printf(ctx->seq, "  backing_dev: %s\n", bdi_dev_name(sb->s_bdi));

	/* 只有块设备型文件系统才有 s_bdev；这里输出底层块设备总字节数。 */
	if (sb->s_bdev)
		seq_printf(ctx->seq, "  block_device_size: %lld bytes\n",
			   (long long)bdev_nr_bytes(sb->s_bdev));

	/*
	 * s_root 是该 superblock 在文件系统内部的根 dentry。先取得 d_name
	 * 快照，再按 d_name.len 输出 d_name.name 中的具体字符串，避免与目录项
	 * 名称变化产生竞态。根 dentry 的名称通常是 "/"；它不是当前进程挂载
	 * 命名空间中的挂载路径，后者保存在 mount 相关结构中。
	 */
	if (sb->s_root) {
		take_dentry_name_snapshot(&root_name, sb->s_root);
		seq_printf(ctx->seq, "  root_dentry_name: '%.*s'\n",
			   (int)root_name.name.len, root_name.name.name);
		release_dentry_name_snapshot(&root_name);
	}

	/* superblock 根目录 inode 的编号及 mode；mode 同时包含类型位和权限位。 */
	if (root_inode)
		seq_printf(ctx->seq, "  root_inode: ino=%lu mode=%#o\n",
			   root_inode->i_ino, (unsigned int)root_inode->i_mode);

	seq_putc(ctx->seq, '\n');
}

static int supers_show(struct seq_file *seq, void *unused)
{
	struct supers_context ctx = {
		.seq = seq,
	};
	iterate_super_fn iterate_super;

	// 从/proc/kallsyms中查询到iterate_supers函数的地址
	iterate_super = (iterate_super_fn)0xffffffffb7ef9660;
	iterate_super(print_super, &ctx);

	/* iterate_supers() 只回调已有根目录的活动 superblock。 */
	seq_printf(seq, "total: %u\n", ctx.count);
	return 0;
}

int super_blocks_show_init(struct proc_dir_entry *dir)
{
	int ret;

	proc_file_supers = proc_file_create_single_p("supers", dir, supers_show);
	if (IS_ERR(proc_file_supers)) {
		ret = PTR_ERR(proc_file_supers);
		proc_file_supers = NULL;
		return ret;
	}

	return 0;
}

void super_blocks_show_exit(void)
{
	if (proc_file_supers)
		proc_file_remove(proc_file_supers);
	proc_file_supers = NULL;

}
