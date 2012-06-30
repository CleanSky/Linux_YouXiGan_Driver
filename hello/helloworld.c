#include <linux/init.h>
#include <linux/module.h>

static int helloworld_init(void){
	printk("Hello World, module init\n");
	return 0;
}

static void helloworld_exit(void){
	printk("Hello World, module exit\n");
}

MODULE_AUTHOR("CleanSky, Zou Bingsong, <zoubingsong#163.com>");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Hello World Module");
MODULE_ALIAS("Hello World Module");

module_init(helloworld_init);
module_exit(helloworld_exit);
