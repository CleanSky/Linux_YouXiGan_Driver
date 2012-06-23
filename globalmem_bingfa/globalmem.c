/*
 * 此文件是增加并发控制后的globalmem设备驱动文件
 */
//包含的一些头文件
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/uaccess.h>

#define GLOBALMEM_SIZE	0x1000	//全局内存大小
#define MEM_CLEAR 0x1		//全局内存清空
#define GLOBALMEM_MAJOR 250	//预设的主设备号

static int globalmem_major = GLOBALMEM_MAJOR;	//主设备号

struct globalmem_dev {
	struct cdev cdev;	//cdev结构体
	unsigned char mem[GLOBALMEM_SIZE];	//全局内存
	struct semaphore sem;	//并发控制用到的信号量
};

struct globalmem_dev *globalmem_devp;

int globalmem_open(struct inode *inode, struct file *filp)
{
	filp->private_data = globalmem_devp;
	return 0;
}

int globalmem_release(struct inode *inode, struct file *filp)
{
	return 0;
}

//增加并发控制后的globalmem设备驱动控制函数
static int globalmem_ioctl(struct inode *inodep, struct file *filp, unsigned
	int cmd, unsigned long arg)
{
	struct globalmem_dev *dev = filp->private_data;	//获得设备结构体指针

	switch (cmd) {
	case MEM_CLEAR:
		//获得信号量
		if (down_interruptible(&dev->sem))
			return  - ERESTARTSYS;
		memset(dev->mem, 0, GLOBALMEM_SIZE);
		up(&dev->sem);//释放信号量

		//输出提示信息
		printk(KERN_INFO "globalmem is set to zero\n");
		break;

	default:
		return  - EINVAL;
	}

	return 0;
}

//增加并发控制后的globalmem读函数
static ssize_t globalmem_read(struct file *filp, char __user *buf, size_t size,
	loff_t *ppos)
{
	unsigned long p =  *ppos;
	unsigned int count = size;
	int ret = 0;
	struct globalmem_dev *dev = filp->private_data;	//获得设备结构体指针

	//分析和获取要读取的有效的长度
	if (p >= GLOBALMEM_SIZE)
		return 0;
	if (count > GLOBALMEM_SIZE - p)
		count = GLOBALMEM_SIZE - p;

	//获得信号量
	if (down_interruptible(&dev->sem))
		return  - ERESTARTSYS;

	//内核空间到用户空间
	if (copy_to_user(buf, (void *)(dev->mem + p), count)) {
		ret =  - EFAULT;
	} else {
		*ppos += count;
		ret = count;

		//输出的提示信息
		printk(KERN_INFO "read %u bytes(s) from %lu\n", count, p);
	}
	up(&dev->sem);	//释放信号量

	return ret;
}

//增加并发控制后的globalmem写函数
static ssize_t globalmem_write(struct file *filp, const char __user *buf,
	size_t size, loff_t *ppos)
{
	unsigned long p =  *ppos;
	unsigned int count = size;
	int ret = 0;
	struct globalmem_dev *dev = filp->private_data;	//获取设备结构体指针

	//分析和获取要写入的有效的长度
	if (p >= GLOBALMEM_SIZE)
		return 0;
	if (count > GLOBALMEM_SIZE - p)
		count = GLOBALMEM_SIZE - p;

	//获得信号量
	if (down_interruptible(&dev->sem))
		return  - ERESTARTSYS;
	
	//用户空间到内核空间
	if (copy_from_user(dev->mem + p, buf, count))
		ret =  - EFAULT;
	else {
		*ppos += count;
		ret = count;

		//输出的提示信息
		printk(KERN_INFO "written %u bytes(s) from %lu\n", count, p);
	}
	up(&dev->sem);//释放信号量

	return ret;
}

static loff_t globalmem_llseek(struct file *filp, loff_t offset, int orig)
{
	loff_t ret = 0;
	switch (orig) {
	case 0:
		if (offset < 0)	{
			ret =  - EINVAL;
			break;
		}
		if ((unsigned int)offset > GLOBALMEM_SIZE) {
			ret =  - EINVAL;
			break;
		}
		filp->f_pos = (unsigned int)offset;
		ret = filp->f_pos;
		break;
	case 1:
		if ((filp->f_pos + offset) > GLOBALMEM_SIZE) {
			ret =  - EINVAL;
			break;
		}
		if ((filp->f_pos + offset) < 0) {
			ret =  - EINVAL;
			break;
		}
		filp->f_pos += offset;
		ret = filp->f_pos;
		break;
	default:
		ret =  - EINVAL;
		break;
	}
	return ret;
}

static const struct file_operations globalmem_fops = {
	.owner = THIS_MODULE,
	.llseek = globalmem_llseek,
	.read = globalmem_read,
	.write = globalmem_write,
	.unlocked_ioctl = globalmem_ioctl,
	.open = globalmem_open,
	.release = globalmem_release,
};

static void globalmem_setup_cdev(struct globalmem_dev *dev, int index)
{
	int err, devno = MKDEV(globalmem_major, index);

	cdev_init(&dev->cdev, &globalmem_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
		printk(KERN_NOTICE "Error %d adding LED%d", err, index);
}

//增加并发控制后的globalmem设备驱动模块加载函数
int globalmem_init(void)
{
	int result;
	dev_t devno = MKDEV(globalmem_major, 0);

	//申请设备号
	if (globalmem_major)
		result = register_chrdev_region(devno, 1, "globalmem");
	else {	//动态申请设备号
		result = alloc_chrdev_region(&devno, 0, 1, "globalmem");
		globalmem_major = MAJOR(devno);
	}
	if (result < 0)
		return result;

	//动态申请设备结构体的内存
	globalmem_devp = kmalloc(sizeof(struct globalmem_dev), GFP_KERNEL);
	if (!globalmem_devp) {	//申请失败
		result =  - ENOMEM;
		goto fail_malloc;
	}

	memset(globalmem_devp, 0, sizeof(struct globalmem_dev));

	globalmem_setup_cdev(globalmem_devp, 0);
	sema_init(&globalmem_devp->sem, 1);	//初始化信号量
	#ifndef init_MUTEX
	sema_init(&globalmem_devp->sem, 1);
	#else
	init_MUTEX(&globalmem);
	#endif
	return 0;

fail_malloc:
	unregister_chrdev_region(devno, 1);
	return result;
}

void globalmem_exit(void)
{
	cdev_del(&globalmem_devp->cdev);
	kfree(globalmem_devp);
	unregister_chrdev_region(MKDEV(globalmem_major, 0), 1);
}

MODULE_LICENSE("Dual BSD/GPL");

module_param(globalmem_major, int, S_IRUGO);

module_init(globalmem_init);
module_exit(globalmem_exit);
