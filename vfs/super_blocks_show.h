/* SPDX-License-Identifier: GPL-2.0 */
#ifndef VFS_SUPER_BLOCKS_SHOW_H
#define VFS_SUPER_BLOCKS_SHOW_H

#include "../utils/utils.h"

int super_blocks_show_init(struct proc_dir_entry *dir);
void super_blocks_show_exit(void);

#endif
