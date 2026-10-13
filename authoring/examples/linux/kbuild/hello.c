// A minimal loadable kernel module: parameters, init/exit, printk. Built here against installed kernel headers; not loaded.
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

static int repeat = 1;
module_param(repeat, int, 0444);
MODULE_PARM_DESC(repeat, "how many times to greet");

static int __init hello_init(void)
{
	for (int i = 0; i < repeat; i++)
		pr_info("hello: greeting %d of %d\n", i + 1, repeat);
	return 0;
}

static void __exit hello_exit(void)
{
	pr_info("hello: goodbye\n");
}

module_init(hello_init);
module_exit(hello_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("DeepNotes example");
MODULE_DESCRIPTION("Minimal module used to show Kbuild");
