
// THIS_MODULE [@include/linux/export.h]
extern struct module __this_module;
#define THIS_MODULE (&__this_module)

// __this_module [@scripts/mod/modpost.c]
static void add_header(struct buffer *b, struct module *mod)
{
	buf_printf(b, "#include <linux/module.h>\n");
	buf_printf(b, "#include <linux/vermagic.h>\n");
	buf_printf(b, "#include <linux/compiler.h>\n");
	buf_printf(b, "\n");
	buf_printf(b, "MODULE_INFO(vermagic, VERMAGIC_STRING);\n");
	buf_printf(b, "\n");
	buf_printf(b, "struct module __this_module\n");
	buf_printf(b, "__attribute__((section(\".gnu.linkonce.this_module\"))) = {\n");
	buf_printf(b, "\t.name = KBUILD_MODNAME,\n");
	if (mod->has_init)
		buf_printf(b, "\t.init = init_module,\n");
	if (mod->has_cleanup)
		buf_printf(b, "#ifdef CONFIG_MODULE_UNLOAD\n"
			      "\t.exit = cleanup_module,\n"
			      "#endif\n");
	buf_printf(b, "\t.arch = MODULE_ARCH_INIT,\n");
	buf_printf(b, "};\n");
}

// kernel用struct module列表来代表各种module
// 由此创建了 xxx.mod.c 文件, 但是module name还是没有找到,

// KBUILD_MODNAME 创建方式如下: [@Makefile]
// -DMACRO=DEFN 以字符串“DEFN”定义 MACRO 宏
gcc -DKBUILD_MODNAME=demo_module ......



