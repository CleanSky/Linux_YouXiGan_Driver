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
#include <linux/poll.h>

#define GLOBALFIFO_SIZE	0x1000	
#define FIFO_CLEAR 0x1  
#define GLOBALFIFO_MAJOR 249    

static int globalfifo_major = GLOBALFIFO_MAJOR;
struct globalfifo_dev {
	struct cdev cdev; 
	unsigned int current_len;    
	unsigned char mem[GLOBALFIFO_SIZE]; 
	struct semaphore sem; 
	wait_queue_head_t r_wait; 
	wait_queue_head_t w_wait; 
	struct fasync_struct *async_queue;	//异步结构体指针，用于读
};

struct globalfifo_dev *globalfifo_devp; 

//支持异步通知的globalfifo设备驱动的fasync函数
static int globalfifo_fasync(int fd, struct file *filp, int mode)
{
	struct globalfifo_dev *dev = filp->private_data; 
	return fasync_helper(fd, filp, mode, &dev->async_queue);
}

int globalfifo_open(struct inode *inode, struct file *filp)
{
	filp->private_data = globalfifo_devp;
	return 0;
}

//增加异步通知后的globalfifo设备驱动的release函数
int globalfifo_release(struct inode *inode, struct file *filp)
{
	globalfifo_fasync( - 1, filp, 0);	//将文件从异步通知队列中删除
	return 0;
}

static int globalfifo_ioctl(struct inode *inodep, struct file *filp, unsigned
		int cmd, unsigned long arg)
{
	struct globalfifo_dev *dev = filp->private_data;

	switch (cmd) {
		case FIFO_CLEAR:
			down(&dev->sem); 
			dev->current_len = 0;
			memset(dev->mem,0,GLOBALFIFO_SIZE);
			up(&dev->sem); 

			printk(KERN_INFO "globalfifo is set to zero\n");
			break;

		default:
			return  - EINVAL;
	}
	return 0;
}

static unsigned int globalfifo_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	struct globalfifo_dev *dev = filp->private_data; 

	down(&dev->sem);

	poll_wait(filp, &dev->r_wait, wait);
	poll_wait(filp, &dev->w_wait, wait);
	if (dev->current_len != 0) {
		mask |= POLLIN | POLLRDNORM; 
	}
	if (dev->current_len != GLOBALFIFO_SIZE) {
		mask |= POLLOUT | POLLWRNORM; 
	}

	up(&dev->sem);
	return mask;
}

//支持异步通知的globalfifo设备驱动的读函数
static ssize_t globalfifo_read(struct file *filp, char __user *buf, size_t count,
		loff_t *ppos)
{
	int ret;
	struct globalfifo_dev *dev = filp->private_data;
	DECLARE_WAITQUEUE(wait, current);

	down(&dev->sem); 
	add_wait_queue(&dev->r_wait, &wait); 

	if (dev->current_len == 0) {
		if (filp->f_flags &O_NONBLOCK) {
			ret =  - EAGAIN;
			goto out;
		}
		__set_current_state(TASK_INTERRUPTIBLE); 
		up(&dev->sem);

		schedule(); 
		if (signal_pending(current)) {
			ret =  - ERESTARTSYS;
			goto out2;
		}

		down(&dev->sem);
	}

	if (count > dev->current_len)
		count = dev->current_len;

	if (copy_to_user(buf, dev->mem, count)) {
		ret =  - EFAULT;
		goto out;
	} else {
		memcpy(dev->mem, dev->mem + count, dev->current_len - count); 
		dev->current_len -= count; 
		printk(KERN_INFO "read %d bytes(s),current_len:%d\n", count, dev->current_len);

		wake_up_interruptible(&dev->w_wait); 

		ret = count;
	}
out:
	up(&dev->sem); 
out2:
	remove_wait_queue(&dev->w_wait, &wait); 
	set_current_state(TASK_RUNNING);
	return ret;
}

//支持异步通知的globalfifo设备驱动的写函数
static ssize_t globalfifo_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *ppos)
{
	struct globalfifo_dev *dev = filp->private_data;	//获得设备结构体指针
	int ret;
	DECLARE_WAITQUEUE(wait, current);	//等待队列的定义

	down(&dev->sem);	//获取信号量
	add_wait_queue(&dev->w_wait, &wait);	//进入写等待队列头

	//等待FIFO非满
	if (dev->current_len == GLOBALFIFO_SIZE) {
		if (filp->f_flags &O_NONBLOCK) {	//非阻塞访问
			ret =  - EAGAIN;
			goto out;
		}
		__set_current_state(TASK_INTERRUPTIBLE);	//改变进程状态为睡眠
		up(&dev->sem);

		schedule();	//调度其他进程执行
		if (signal_pending(current)) {	//如果是因为信号唤醒
			ret =  - ERESTARTSYS;
			goto out2;
		}

		down(&dev->sem);	//获得信号量
	}

	//用户空间到内核空间
	if (count > GLOBALFIFO_SIZE - dev->current_len)
		count = GLOBALFIFO_SIZE - dev->current_len;

	if (copy_from_user(dev->mem + dev->current_len, buf, count)) {
		ret =  - EFAULT;
		goto out;
	} else {
		dev->current_len += count;
		printk(KERN_INFO "written %d bytes(s),current_len:%d\n", count, dev
				->current_len);

		wake_up_interruptible(&dev->r_wait);	//唤醒读等待队列
		//产生异步读信号
		if (dev->async_queue) {
			kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
			printk(KERN_DEBUG "%s kill SIGIO\n", __func__);
		}

		ret = count;
	}

out:
	up(&dev->sem);	//释放信号量
out2:
	remove_wait_queue(&dev->w_wait, &wait);	//从附属的等待队列头移除
	set_current_state(TASK_RUNNING);
	return ret;
}

static const struct file_operations globalfifo_fops = {
	.owner = THIS_MODULE,
	.read = globalfifo_read,
	.write = globalfifo_write,
	.unlocked_ioctl = globalfifo_ioctl,
	.poll = globalfifo_poll,
	.fasync = globalfifo_fasync,
	.open = globalfifo_open,
	.release = globalfifo_release,
};

static void globalfifo_setup_cdev(struct globalfifo_dev *dev, int index)
{
	int err, devno = MKDEV(globalfifo_major, index);

	cdev_init(&dev->cdev, &globalfifo_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
		printk(KERN_NOTICE "Error %d adding LED%d", err, index);
}

int globalfifo_init(void)
{
	int ret;
	dev_t devno = MKDEV(globalfifo_major, 0);

	if (globalfifo_major)
		ret = register_chrdev_region(devno, 1, "globalfifo");
	else  { 
		ret = alloc_chrdev_region(&devno, 0, 1, "globalfifo");
		globalfifo_major = MAJOR(devno);
	}
	if (ret < 0)
		return ret;
	globalfifo_devp = kmalloc(sizeof(struct globalfifo_dev), GFP_KERNEL);
	if (!globalfifo_devp) {
		ret =  - ENOMEM;
		goto fail_malloc;
	}

	memset(globalfifo_devp, 0, sizeof(struct globalfifo_dev));

	globalfifo_setup_cdev(globalfifo_devp, 0);

	sema_init(&globalfifo_devp->sem, 1);   
	#ifndef init_MUTEX
	sema_init(&globalfifo_devp->sem, 1);
	#else
	init_MUTEX(&globalfifo_devp->sem);
	#endif
	init_waitqueue_head(&globalfifo_devp->r_wait); 
	init_waitqueue_head(&globalfifo_devp->w_wait); 

	return 0;

fail_malloc: unregister_chrdev_region(devno, 1);
	     return ret;
}

void globalfifo_exit(void)
{
	cdev_del(&globalfifo_devp->cdev);   
	kfree(globalfifo_devp);     
	unregister_chrdev_region(MKDEV(globalfifo_major, 0), 1); 
}

MODULE_LICENSE("Dual BSD/GPL");

module_param(globalfifo_major, int, S_IRUGO);

module_init(globalfifo_init);
module_exit(globalfifo_exit);