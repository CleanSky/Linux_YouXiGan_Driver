#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/wait.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
//#include <plat/gpio.h>
#include <linux/moduleparam.h>

MODULE_LICENSE ("GPL");

#define DEVICE_NAME "switch"

#define zhuban	0x20000000
#define SYS_STATUS (zhuban+(0x20<<1))
#define DIP_SW_L 0x00000001

#define INTCPS_REVISION  0x48200000

static volatile unsigned int *	intcps_revision;

static volatile unsigned int *	sys_status;
static volatile unsigned long*	dip_l;
static volatile unsigned short *	dip_h;

int switch_major = 3000;
module_param(switch_major,int,S_IRUGO);
int switch_minor = 0;
int number_of_devices = 1;

struct cdev cdev;
dev_t devno = 0;

struct class *key_class;

ssize_t switch_read (struct file *filp, char *buff, size_t count, loff_t *offp)
{
	unsigned int temp;

	temp =  __raw_readw(dip_l);
	if(copy_to_user(buff,&temp,sizeof(temp)))
	{
		return -EFAULT;
	}

	return sizeof(temp);
}

ssize_t switch_write (struct file *filp, const char  *buf, size_t count, loff_t *f_pos)
{
	ssize_t ret = 0;
	
	
	return ret;
}

static int switch_open (struct inode *inode, struct file *file)
{
	
	__raw_writew((__raw_readw(sys_status))|(1<<2),sys_status);
	
	return 0;
}

static int switch_release (struct inode *inode, struct file *file)
{
		
	__raw_writew((__raw_readw(sys_status))&(0xfffb),sys_status);
	
	return 0;
}

struct file_operations switch_fops = {
	.owner   = THIS_MODULE,
	.open    = switch_open,
	.release = switch_release,
	.read  	 = switch_read,
	.write   = switch_write
};

static int __init switch_init (void)
{
	int status;
	devno = MKDEV (switch_major, switch_minor);
	status = register_chrdev_region (devno, number_of_devices, DEVICE_NAME);
	if (status<0) {
		printk (KERN_WARNING "Can't register major number:%d,check /dev device_NO.\n", switch_major);
		goto out1;
	}
		
	intcps_revision		=	ioremap(INTCPS_REVISION,0x4);
	sys_status		=	ioremap(SYS_STATUS,0x2);
	dip_l			=	ioremap(DIP_SW_L,0x4);
	//dip_h			=	ioremap(DIP_SW_H,0x2);
		
	cdev_init (&cdev, &switch_fops);
	cdev.owner = THIS_MODULE;
	cdev.ops = &switch_fops;
	status = cdev_add (&cdev, devno , 1);
	if (status) {
		printk (KERN_NOTICE "Error %d adding char_reg_setup_cdev", status);
		goto out2;
	}
	
	key_class = class_create(THIS_MODULE, DEVICE_NAME);
	device_create(key_class,NULL,devno,NULL,DEVICE_NAME);
	
	printk("Drivers OK,welcome to test!\n");
	
	return 0;

out2:
	unregister_chrdev_region (devno, number_of_devices);
out1:
	return status;

}

static void __exit switch_exit (void)
{
	dev_t devno = MKDEV (switch_major, switch_minor);

	cdev_del (&cdev);
		
	iounmap(sys_status);
	
	iounmap(dip_l);
	//iounmap(dip_h);
	
	iounmap(intcps_revision);
	
	device_destroy(key_class,devno);
	class_destroy(key_class); 
	unregister_chrdev_region (devno, number_of_devices);
	
	printk (KERN_INFO "char driver cleaned up\n");
}

module_init (switch_init);
module_exit (switch_exit);

