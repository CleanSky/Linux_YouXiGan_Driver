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
#include <plat/gpio.h>
#include <linux/moduleparam.h>

#define DEVICE_NAME "seg"

#define FPGA_BASE 0x20000000  //物理页面偏移地址，在电脑上是0xc000_0000;
#define SYS_STATUS (FPGA_BASE+(0x20<<1))
#define DIP_SW_L (FPGA_BASE+(0x12<<1))//设备物理低地址左移变为16位
#define DIP_SW_H (FPGA_BASE+(0x13<<1))//设备高地址

#define  L0 (0x3f)//写出不同数字的编码
#define  L1 (0x06)
#define  L2 (0x5b)
#define  L3 (0x4f)
#define  L4 (0x66)
#define  L5 (0x6d)
#define  L6 (0x7d)
#define  L7 (0x07)
#define  L8 (0x7f)
#define  L9 (0x6f)

#define INTCPS_REVISION  0x48200000
static volatile unsigned int *	intcps_revision;

static volatile unsigned short *	sys_status;
static volatile unsigned short *	dip_l;
static volatile unsigned short *	dip_h;

int hello_major = 234;
module_param(hello_major,int,S_IRUGO);
int hello_minor = 0;
int number_of_devices = 1;

struct cdev cdev;
dev_t devno = 0;

struct class *key_class;

void  only_test (void)
{
	int i;

	for(i=0;i<=9;++i)
	{
		printk("i:%#x\n",i);

		switch(i){
		case 0 :
		__raw_writew(L0|L0<<8,dip_l);	__raw_writew(L0|L0<<8,dip_h);//写入设备，这个是write函数调用的底层函数
		break;

		case 1 :
		__raw_writew(L1|L1<<8,dip_l);	__raw_writew(L1|L1<<8,dip_h);
		break;
		
		case 2 :
		__raw_writew(L2|L2<<8,dip_l);	__raw_writew(L2|L2<<8,dip_h);
		break;
		
		case 3 :
		__raw_writew(L3|L3<<8,dip_l);	__raw_writew(L3|L3<<8,dip_h);
		break;
		
		case 4 :
		__raw_writew(L4|L4<<8,dip_l);	__raw_writew(L4|L4<<8,dip_h);
		break;
		
		case 5 :
		__raw_writew(L5|L5<<8,dip_l);	__raw_writew(L5|L5<<8,dip_h);
		break;
		
		case 6 :
		__raw_writew(L6|L6<<8,dip_l);	__raw_writew(L6|L6<<8,dip_h);
		break;
		
		case 7 :
		__raw_writew(L7|L7<<8,dip_l);	__raw_writew(L7|L7<<8,dip_h);
		break;
		
		case 8 :
		__raw_writew(L8|L8<<8,dip_l);	__raw_writew(L8|L8<<8,dip_h);
		break;
		
		case 9 :
		__raw_writew(L9|L9<<8,dip_l);	__raw_writew(L9|L9<<8,dip_h);
		break;
		
		
		default:
		break;
		}
		msleep(1000);
	}
	
}


static unsigned short  translate (unsigned short num)
{
	unsigned short temp = 0;
	
	switch (num/10)
	{
	case 0:
		temp = L0;
		break;
		
	case 1:
		temp = L1;
		break;
		
	case 2:
		temp = L2;
		break;
		
	case 3:
		temp = L3;
		break;
		
	case 4:
		temp = L4;
		break;
		
	case 5:
		temp = L5;
		break;
		
	case 6:
		temp = L6;
		break;
		
	case 7:
		temp = L7;
		break;
		
	case 8:
		temp = L8;
		break;
		
	case 9:
		temp = L9;
		break;
		
	default:
		printk("error\n");
		break;
	}
	
	switch (num%10)
	{
	case 0:
		temp |= L0<<8;
		break;
		
	case 1:
		temp |= L1<<8;
		break;
		
	case 2:
		temp |= L2<<8;
		break;
		
	case 3:
		temp |= L3<<8;
		break;
		
	case 4:
		temp |= L4<<8;
		break;
		
	case 5:
		temp |= L5<<8;
		break;
		
	case 6:
		temp |= L6<<8;
		break;
		
	case 7:
		temp |= L7<<8;
		break;
		
	case 8:
		temp |= L8<<8;
		break;
		
	case 9:
		temp |= L9<<8;
		break;
		
	default:
		printk("error\n");
		break;
	}
	
	return temp;
}

ssize_t seg_read (struct file *filp, char *buff, size_t count, loff_t *offp)
{
	ssize_t result = 0;

	return result;
}

ssize_t seg_write (struct file *filp, const char  *buff, size_t count, loff_t *f_pos)
{
	unsigned short num,temp;
	
	if(copy_from_user(&num,buff,sizeof(num)))
	{
		return -EFAULT;
	}
		
	temp = translate (num%100);
	
	__raw_writew(temp,dip_h);
	
	temp = translate (num/100);
	
	__raw_writew(temp,dip_l);
	
	return sizeof(temp);
}

static int seg_open (struct inode *inode, struct file *file)
{
	return 0;
}

static int seg_release (struct inode *inode, struct file *file)
{
	
	__raw_writew(0x3f|0x3f<<8,dip_l);
	__raw_writew(0x3f|0x3f<<8,dip_h);
	
	return 0;
}

struct file_operations seg_fops = {
	.owner = THIS_MODULE,
	.open  = seg_open,
	.release = seg_release,
	.read  = seg_read,
	.write = seg_write
};

static int __init seg_init (void)
{
	int status;
	devno = MKDEV (hello_major, hello_minor);
	status = register_chrdev_region (devno, number_of_devices, DEVICE_NAME);
	if (status<0) {
		printk (KERN_WARNING "Can't register major number:%d,check /dev device_NO.\n", hello_major);
		goto out1;
	}
		
	intcps_revision		=	ioremap(INTCPS_REVISION,0x4);
	sys_status		=	ioremap(SYS_STATUS,0x2);
	dip_l			=	ioremap(DIP_SW_L,0x2); //将设备物理地址对应到内核内存虚拟地址。0x2是大小
	dip_h			=	ioremap(DIP_SW_H,0x2); //

	__raw_writew((__raw_readw(sys_status))|0x1,sys_status);
	__raw_writew(0x3f|0x3f<<8,dip_l);
	__raw_writew(0x3f|0x3f<<8,dip_h);

	cdev_init (&cdev, &seg_fops);
	cdev.owner = THIS_MODULE;
	cdev.ops = &seg_fops;
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

static void __exit seg_exit (void)
{
	dev_t devno = MKDEV (hello_major, hello_minor);
		
	__raw_writew((__raw_readw(sys_status))&(0xfffe),sys_status);

	
	cdev_del (&cdev);
		
	iounmap(sys_status);
	
	iounmap(dip_l); // 注销内存
	iounmap(dip_h); //
	
	iounmap(intcps_revision);
		
	device_destroy(key_class,devno);
	class_destroy(key_class); 
	unregister_chrdev_region (devno, number_of_devices);
	
	printk (KERN_INFO "char driver cleaned up\n");
}

module_init (seg_init);
module_exit (seg_exit);

MODULE_LICENSE ("GPL");

