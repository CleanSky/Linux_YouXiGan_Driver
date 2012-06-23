/*
 *本文件是一个模块测试文件
 *加载hello.ko模块：sudo insmod helloworld.ko
 *卸载hello模块：sudo rmmod helloworld
*/
//包含的一些头文件
#include <linux/init.h>	
#include <linux/module.h>

static int helloworld_init(void){		//模块加载函数
	printk("Hello World enter\n");		//模块加载时输出的信息
	return 0;
}

static void helloworld_exit(void){		//模块卸载函数
	printk("Hello World exit\n");		//模块卸载时输出的信息
}

module_init(helloworld_init);			//模块加载
module_exit(helloworld_exit);			//模块卸载

MODULE_AUTHOR("CleanSky <zoubingsong@163.com>");//模块作者信息
MODULE_LICENSE("Dual BSD/GPL");			//模块使用的开放协议
MODULE_DESCRIPTION("A simple Hello World Module");//模块描述信息
MODULE_ALIAS("A simplest module");		//模块说明
