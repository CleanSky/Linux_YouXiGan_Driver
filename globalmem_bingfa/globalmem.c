#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/uaccess.h>

#define SIZE 1024
#define CLEAR 1
#define MAJOR 250

static int globalmem_major = MAJOR;

struct globalmem_dev{
	struct cdev cdev;
	char mem[SIZE];
	struct semaphore sem;
};

struct globalmem_dev *globalmem;

static int globalmem_open(struct inode *inode, struct file *fp)
{
	fp->private_data = globalmem;
	printk("Open Successful!");
	
	return 0;
}

static int globalmem_release(struct inode *inode, struct file *fp)
{
	printk("Release Successful!");

	return 0;
}

static ssize_t globalmem_read(struct file *fp, char __user *buf, size_t size, loff_t *position)
{
	int result = 0;
	long pos = *position;
	int length = size;

	struct globalmem_dev *dev = fp->private_data;

	if(pos >= SIZE){
		printk("Invalid Read Position!\n");
		return 0;
	}

	if(length > SIZE - pos){
		length = SIZE - pos;
	}
	
	if(down_interruptible(&dev->sem)){
		return -ERESTARTSYS;
	}

	if(!copy_to_user(buf, (void *)(dev->mem + pos), length)){
		*position += length;
		result = length;
		printk("Read %d bytes from %lu\n", length, pos);
	}else{
		printk("Read Failure!\n");
		result = -EFAULT;
	}
	up(&dev->sem);

	return result;
}

static ssize_t globalmem_write(struct file *fp, const char __user *buf, size_t size, loff_t *position)
{
	int result = 0;
	long pos = *position;
	int length = size;
	
	struct globalmem_dev *dev = fp->private_data;

	if(pos >= SIZE){
		printk("Invalid Write Position!\n");
		return 0;
	}

	if(length > SIZE - pos){
		length = SIZE - pos;
	}
	
	if(down_interruptible(&dev->sem)){
		return -ERESTARTSYS;
	}

	if(!copy_from_user((void *)dev->mem + pos, buf, length)){
		*position += length;
		result = length;
		printk("Write %d bytes from %lu\n", length, pos);
	}
	up(&dev->sem);

	return result;
}

static int globalmem_ioctl(struct file *fp, int cmd, long arg)
{
	struct globalmem_dev *dev = fp->private_data;
	
	switch(cmd){
	case CLEAR:
		if(down_interruptible(&dev->sem)){
			return -ERESTARTSYS;
		}
		memset(dev->mem, 0, SIZE);
		up(&dev->sem);
		printk("Globalmem is clean\n");
		break;
	default:
		return -EINVAL;
	}
	
	return 0;
}

static loff_t globalmem_llseek(struct file *fp, loff_t offset, int origin)
{
	loff_t result = 0;

	switch(origin){
	case 0:
		if(offset < 0){
			result = -EINVAL;
			break;
		}
		if((int)offset > SIZE){
			result = -EINVAL;
			break;
		}
		fp->f_pos = (int)offset;
	
		result = fp->f_pos;
		break;
	case 1:
		if((fp->f_pos + offset) > SIZE){
			result  = -EINVAL;
			break;
		}
		if((fp->f_pos + offset) < 0){
			result = -EINVAL;
			break;
		}
		fp->f_pos += offset;
		result = fp->f_pos;
		break;
	default:
		result = -EINVAL;
		break;
	}

	return result;
}

static const struct file_operations globalmem_op = {
	.owner = THIS_MODULE,
	.open = globalmem_open,
	.release = globalmem_release,
	.read = globalmem_read,
	.write = globalmem_write,
	.unlocked_ioctl = globalmem_ioctl,
	.llseek = globalmem_llseek,
};

static void globalmem_setup_cdev(struct globalmem_dev *dev, int index)
{
	int devnumber = MKDEV(globalmem_major, index);
	
	cdev_init(&dev->cdev, &globalmem_op);
	dev->cdev.owner = THIS_MODULE;
	if(!cdev_add(&dev->cdev, devnumber, 1)){
		printk("Globalmem Setup Finished!\n");
	}else{
		printk("Globalmem Setup Error!\n");
	}
}

static int globalmem_init(void)
{
	int result = 0;
	dev_t devnumber = MKDEV(globalmem_major, 0);

	result = register_chrdev_region(devnumber, 1, "globalmem");
	if(result < 0){
		return result;
	}

	globalmem = kmalloc(sizeof(struct globalmem_dev), GFP_KERNEL);
	if(!globalmem){
		printk("Globalmem Kmalloc Failure!\n");
		result = -ENOMEM;
		goto fail_malloc;
	}

	memset(globalmem, 0, sizeof(struct globalmem_dev));

	globalmem_setup_cdev(globalmem, 0);

	sema_init(&globalmem->sem, 1);
	#ifndef init_MUTEX
	sema_init(&globalmem->sem, 1);
	#else
	init_MUTEX(&globalmem);
	#endif
	return 0;

fail_malloc:
	unregister_chrdev_region(devnumber, 1);
	return result;
}

static void globalmem_exit(void)
{
	cdev_del(&globalmem->cdev);
	kfree(globalmem);
	unregister_chrdev_region(MKDEV(globalmem_major, 0), 1);
}

MODULE_AUTHOR("Clean Sky, Zou Bingsong<zoubingsong@163.com>");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Globalmem Driver");
MODULE_ALIAS("Globalmem");

module_init(globalmem_init);
module_exit(globalmem_exit);
