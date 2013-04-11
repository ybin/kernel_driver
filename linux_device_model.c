/*
 * LDM - Linux Device Model
 * 		for linux-3.8
 */

// 0. Here we go.
// [@init/main.c]
/*
 * Ok, the machine is now initialized. None of the devices
 * have been touched yet, but the CPU subsystem is up and
 * running, and memory and process management works.
 *
 * Now we can finally start doing some real work..
 */
static void __init do_basic_setup(void)
{
	cpuset_init_smp();
	usermodehelper_init();
	shmem_init();
	// *** this is the entrance of 'drivers'
	driver_init();
	init_irq_proc();
	do_ctors();
	usermodehelper_enable();
	do_initcalls();
}

// 1. dirver initialization
// [@drivers/base/init.c]
/**
 * driver_init - initialize driver model.
 *
 * Call the driver model init functions to initialize their
 * subsystems. Called early from init/main.c.
 */
void __init driver_init(void)
{
	/* These are the core pieces */
	// 初始化tmpfs - sysfs的替代品？
	devtmpfs_init();
	// device, bus, class - 多么熟悉的名字啊！
	devices_init();
	buses_init();
	classes_init();
	// 哦... 好吧，下面的内容我们暂且忽略好了， :)
	firmware_init();
	hypervisor_init();

	/* These are also core pieces, but must come after the
	 * core core pieces.
	 */
	platform_bus_init();
	cpu_dev_init();
	memory_dev_init();
}

// 2. 焦点聚焦到 devices_init(), buses_init(), classes_init() 身上
// 2.1 device initialization
// [@drivers/base/core.c]
/* /sys/devices/ */
struct kset *devices_kset;
static struct kobject *dev_kobj;
struct kobject *sysfs_dev_char_kobj;
struct kobject *sysfs_dev_block_kobj;


int __init devices_init(void)
{
	devices_kset = kset_create_and_add("devices", &device_uevent_ops, NULL);
	dev_kobj = kobject_create_and_add("dev", NULL);
	sysfs_dev_block_kobj = kobject_create_and_add("block", dev_kobj);
	sysfs_dev_char_kobj = kobject_create_and_add("char", dev_kobj);

	return 0;
}

// 2.2 bus initialization
// [@drivers/base/bus.c]
static struct kset *bus_kset;
/* /sys/devices/system */
static struct kset *system_kset;

int __init buses_init(void)
{
	bus_kset = kset_create_and_add("bus", &bus_uevent_ops, NULL);
	if (!bus_kset)
		return -ENOMEM;

	system_kset = kset_create_and_add("system", NULL, &devices_kset->kobj);
	if (!system_kset)
		return -ENOMEM;

	return 0;
}

// 2.3 class initialization
// [@drivers/base/class.c]
static struct kset *class_kset;

int __init classes_init(void)
{
	class_kset = kset_create_and_add("class", NULL, NULL);
	if (!class_kset)
		return -ENOMEM;
	return 0;
}

// 3. 好吧，看来下面两个函数很关键啊！
// 3.1 kset_create_and_add()
// [@lib/kobject.c]
struct kset *kset_create_and_add(const char *name,
				 const struct kset_uevent_ops *uevent_ops,
				 struct kobject *parent_kobj)
{
	struct kset *kset;
	int error;

	kset = kset_create(name, uevent_ops, parent_kobj);
	if (!kset)
		return NULL;
	error = kset_register(kset);
	if (error) {
		kfree(kset);
		return NULL;
	}
	return kset;
}

// 3.2 kobject_create_and_add()
// [@lib/kobject.c]
struct kobject *kobject_create_and_add(const char *name, struct kobject *parent)
{
	struct kobject *kobj;
	int retval;

	kobj = kobject_create();
	if (!kobj)
		return NULL;

	retval = kobject_add(kobj, parent, "%s", name);
	if (retval) {
		printk(KERN_WARNING "%s: kobject_add error: %d\n",
		       __func__, retval);
		kobject_put(kobj);
		kobj = NULL;
	}
	return kobj;
}
