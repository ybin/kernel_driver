
// 0. module_init macro [@include/linux/init.h]
#define module_init(x)	__initcall(x);

// 1. __initcall macro [@include/linux/init.h]
#define __initcall(fn) device_initcall(fn)

// 2. device_initcall macro [@include/linux/init.h]
#define device_initcall(fn)		__define_initcall("6",fn,6)

// 3. __define_initcall macro [@include/linux/init.h]
// __attribute__((__section__("section-name"))) 表示
// 把符号放置到"section-name"段
// __used 表示该符号必须有定义, 没音定义的话编译器就会报错,
// __unused 表示符号定义可有可无, 没有定义的话编译器不会报错
// 具体到这里的解释是(以vivi_init函数为例):
// 		static initcall_t __initcall_vivi_init6 = vivi_init;
// 符号 __initcall_vivi_init6 必须要有定义, 否则编译器报错,
// 而且该符号要放置到 .initcall6.init 段中.
#define __define_initcall(level,fn,id) \
	static initcall_t __initcall_##fn##id __used \
	__attribute__((__section__(".initcall" level ".init"))) = fn

