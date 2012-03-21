/*
 *sudo insmod ./hello.ko
 *sudo rmmode hello
*/
#include <linux/init.h>								//头文件
#include <linux/module.h>

static int helloworld_init(void){					//模块加载函数
	printk("Hello World enter\n");					//模块加载时输出的信息
	return 0;
}

static void helloworld_exit(void){					//模块卸载函数
	printk("Hello World exit\n");					//模块卸载时输出的信息
}

module_init(helloworld_init);						//模块加载
module_exit(helloworld_exit);						//模块卸载

MODULE_AUTHOR("CleanSky <zoubingsong@163.com>");	//模块声明与描述
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("A simple Hello World Module");
MODULE_ALIAS("A simplest module");
