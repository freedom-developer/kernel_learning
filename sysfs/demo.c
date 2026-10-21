// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/sysfs.h>

#define SYSFS_DEMO_DIR "sysfs_demo"

static struct kobject *sysfs_demo_kobj;
static int demo_value = 100;
static bool demo_enabled = true;

static ssize_t value_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	return sysfs_emit(buf, "%d\n", READ_ONCE(demo_value));
}

static ssize_t value_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t count)
{
	int value;
	int ret;

	ret = kstrtoint(buf, 0, &value);
	if (ret)
		return ret;

	WRITE_ONCE(demo_value, value);
	return count;
}

static ssize_t enabled_show(struct kobject *kobj,
			    struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%u\n", READ_ONCE(demo_enabled));
}

static ssize_t enabled_store(struct kobject *kobj,
			     struct kobj_attribute *attr,
			     const char *buf, size_t count)
{
	bool enabled;
	int ret;

	ret = kstrtobool(buf, &enabled);
	if (ret)
		return ret;

	WRITE_ONCE(demo_enabled, enabled);
	return count;
}

static ssize_t description_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "sysfs attribute callback demo\n");
}

static struct kobj_attribute value_attr = __ATTR_RW(value);
static struct kobj_attribute enabled_attr = __ATTR_RW(enabled);
static struct kobj_attribute description_attr = __ATTR_RO(description);

static struct attribute *sysfs_demo_attrs[] = {
	&value_attr.attr,
	&enabled_attr.attr,
	&description_attr.attr,
	NULL,
};

static const struct attribute_group sysfs_demo_group = {
	.attrs = sysfs_demo_attrs,
};

static int __init sysfs_demo_init(void)
{
	int ret;

	sysfs_demo_kobj = kobject_create_and_add(SYSFS_DEMO_DIR, kernel_kobj);
	if (!sysfs_demo_kobj)
		return -ENOMEM;

	ret = sysfs_create_group(sysfs_demo_kobj, &sysfs_demo_group);
	if (ret) {
		kobject_put(sysfs_demo_kobj);
		sysfs_demo_kobj = NULL;
		return ret;
	}

	pr_info("sysfs_demo: created /sys/kernel/%s\n", SYSFS_DEMO_DIR);
	return 0;
}

static void __exit sysfs_demo_exit(void)
{
	if (sysfs_demo_kobj) {
		sysfs_remove_group(sysfs_demo_kobj, &sysfs_demo_group);
		kobject_put(sysfs_demo_kobj);
		sysfs_demo_kobj = NULL;
	}

	pr_info("sysfs_demo: removed /sys/kernel/%s\n", SYSFS_DEMO_DIR);
}

module_init(sysfs_demo_init);
module_exit(sysfs_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("kernel_learning");
MODULE_DESCRIPTION("Sysfs attribute demo module");
