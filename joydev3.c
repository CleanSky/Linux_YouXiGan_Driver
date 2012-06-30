#include <asm/io.h>
#include <asm/system.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/joystick.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/device.h>

MODULE_AUTHOR("wangyashan"); /*模块作者*/
MODULE_DESCRIPTION("Joystick device interfaces"); /*所支持设备的简单描述*/
MODULE_SUPPORTED_DEVICE("input/js");/*模块所支持的设备类型*/
MODULE_LICENSE("GPL"); /*内核模块使用的许可证*/


#define JOYDEV_MINOR_BASE 0
#define JOYDEV_MINORS 16
#define JOYDEV_BUFFER_SIZE 64  //最多存取的中断数，即event的数目。

struct joydev{
       int open;/*记录设备文件被打开的次数*/
       int minor; /*给连接的设备分配的次设备号*/
       struct input_handle handle;/* 结构体handle中有一个
                                 设备结构体和一个handler结构体
                                  中断发生时，会先查找设备中的handle_list中
                                 的第一个激活的 handle,然后再根据handle
                                 再找到中断处理器handler。所以handle结构体中
                                 会有一个设备dev结构体，和一个handler结构体。
                                 handle将dev和handler结合了起来。*/
      
      struct list_head client_list;
      spinlock_t client_lock;/*设备链表自旋锁*/
      struct mutex mutex; /*设备互斥锁*/
      struct device dev;/*设备结构体*/
      bool exist;   /*设备是否断开*/
    struct  js_corr corr[ABS_CNT];
    //struct   JS_DATA_SAVE_TYPE glue; /*和js_corr一起在joydtick.h中定义*/
    int nabs;/*方位数*/
    int nkey;/*按键数*/
     /*BTN_MISC=0X100
       KEY_MAX=0x2ff
       ABS_CNT=0x40
       BTN_STICK=0x120*/
    __u16 keymap[KEY_MAX - BTN_MISC + 1];
    __u16 keypam[KEY_MAX - BTN_MISC + 1];
    __u8 absmap[ABS_CNT]; 
    __u8 abspam[ABS_CNT];
/*上面四个数组分别存储按键和方位到位置，和位置到按键和方位的映射值。*/
    __s16 abs[ABS_CNT];/*存储摇杆的方位值*/

};

struct joydev_client {  /* cunfang zhong duan event*/
	struct js_event buffer[JOYDEV_BUFFER_SIZE];/*中断数*/
	int head; 
	int tail;
	int startup;/*发生中断数*/
	spinlock_t buffer_lock; /** 保护buffer */
	struct joydev *joydev; /*设备*/
	struct list_head node; /* 未知*/
};

static struct joydev *joydev_table[JOYDEV_MINORS];/*驱动支持的设备数组*/
static DEFINE_MUTEX(joydev_table_mutex);/*锁*/

static void joydev_pass_event(struct joydev_client *client , struct js_event *event){
         struct joydev *joydev = client->joydev;

           spin_lock(&client->buffer_lock);/*中断不能用了，只能用锁*/

           client->buffer[client->head]=*event;
           if(client->startup == joydev->nabs+joydev->nkey){
			client->head++;
			client->head &= JOYDEV_BUFFER_SIZE-1;/*0~63*/
                        if(client->head==client->tail)
				client->startup=0;
 			}
          spin_unlock(&client->buffer_lock);
	  //kill_fasync(&client->fasync,SIGIO,POLL_IN);/*异步通知，向fasync发送可读信号*/
			
	}

static int joydev_correct(int value, struct js_corr *corr)
{
	switch (corr->type) {

	case JS_CORR_NONE:
		break;

	case JS_CORR_BROKEN:
		value = value > corr->coef[0] ? (value < corr->coef[1] ? 0 :
			((corr->coef[3] * (value - corr->coef[1])) >> 14)) :
			((corr->coef[2] * (value - corr->coef[0])) >> 14);
		break;

	default:
		return 0;
	}

	return value < -32767 ? -32767 : (value > 32767 ? 32767 : value);
}
static void joydev_event(struct input_handle *handle , unsigned int type , unsigned int 
code, int value){
	struct joydev *joydev = handle->private;
        struct joydev_client  *client;
        struct js_event event;
        switch(type){
             case EV_KEY:
                          /*value的值只可能为0,1,0表示按下，1表示跳起*/
			if(code < BTN_MISC || value>1)return ;
                      event.type = JS_EVENT_BUTTON; /*JS_EVENT_BUTTON=1*/
                      event.number = joydev->keymap[code - BTN_MISC];
                      event.value = value;
                      break;
  	     case EV_ABS:
                    event.type = JS_EVENT_AXIS; /*JS_EVENT_AXIS=2*/
		    event.number = joydev->absmap[code];
                    event.value = joydev_correct(value , &joydev->corr[event.number]);
                     /*下面这2句话的意思，将摇杆和方向按钮的值对应。变为一样*/
	            if(event.value==joydev->abs[event.number])
                                  return ;
                     joydev->abs[event.number] = event.value;
                           break;
             default:
                       return ;
		}
    		event.time = jiffies_to_msecs(jiffies);/*当前系统时间*/
                     //printk("444time:%u,value:%x,type:%x,number:%x
//        \n",event.time,event.value,event.type,event.number);
    if(event.type==1 && event.value==1){
          switch(event.number){
                 case 0: printk("press 1\n");break;
                 case 1: printk("press 2\n");break;
                 case 2: printk("press 3\n");break;
                 case 3: printk("press 4\n");break;
                 case 4: printk("press left 1\n");break;
                 case 5: printk("press right 1\n");break;
                 case 6: printk("press left 2\n");break;
                 case 7: printk("press right 2\n");break;
                 case 8: printk("press select \n");break;
                 case 9: printk("press start\n");break;
                 default : ;
            }
          
      } 
        else if(event.type==2){
           switch(event.value){
             case 0x7fff: if(event.number==0)printk("press right\n");
                        else if(event.number==1)printk("press down\n");
                      break;
             case 0xffff8001 : if(event.number==0)printk("press left\n");
                        else if(event.number==1)printk("press up\n");
                      break;
             default : ;
            }
        }
        rcu_read_lock();
	list_for_each_entry_rcu(client, &joydev->client_list, node)
		joydev_pass_event(client, &event);
	rcu_read_unlock();

	
            
}	

static void joydev_free(struct device *dev)
{
	struct joydev *joydev = container_of(dev, struct joydev, dev);/*根据一个结构体变
量中的一个域成员变量的指针来获取指向整个结构体变量的指针,*/

	input_put_device(joydev->handle.dev);
       /*释放内存区域*/
	kfree(joydev);
}
/*把joydev_client中的node 加入到joydev中的client_list链表中*/
static void joydev_attach_client(struct joydev *joydev , struct joydev_client *client)
{
	spin_lock(&joydev->client_lock);
        /*static inline void list_add_rcu(struct list_head *new, struct list_head *head) 
该函数把链表项new插入到RCU保护的链表head的开头。使用内存栅保证了在引用这个新插入的链表项
之前，新链表项的链接指针的修改对所有读者是可见的。*/
        list_add_tail_rcu(&client->node , &joydev->client_list);
        spin_unlock(&joydev->client_lock);
        synchronize_rcu();
}
/*很显然这个函数和attach相对应，即从joydev中的client_list中删除node表项*/
static void joydev_detach_client(struct joydev *joydev , struct joydev_client *client)
{
	spin_lock(&joydev->client_lock);
        list_del_rcu(&client->node);
        spin_unlock(&joydev->client_lock);
 /*该函数由RCU写端调用，它将阻塞写者，直到经过grace period后，即所有的读者已经完成读端临
界区，写者才可以继续下一步操作。如果有多个RCU写端调用该函数，他们将在一个grace period之后
全部被唤醒。*/
        synchronize_rcu();
    
}
/*先给joydev上锁，若设备已经不存在，返回错误
  若存在，且是第一个打开，就调用input_open_device()函数
  主要是打开joydev对应的handle中的设备。返回retval。
  */
static int joydev_open_device(struct joydev *joydev){
		int retval;
		retval=mutex_lock_interruptible(&joydev->mutex);
		if(retval)return retval;
		if(!joydev->exist)retval= -ENODEV;
		else if(!joydev->open++){
                           /*如果是第一次打开设备，就调用input_open_device()
                             主要还是打开handle中对应的设备。*/
			retval = input_open_device(&joydev->handle);
                              if(retval)joydev->open--;
 			}
		mutex_unlock(&joydev->mutex);
                return retval;
}
/*这个函数与open相对，就是关闭joydev对应的handle中的设备。*/
static void joydev_close_device(struct joydev *joydev){
	mutex_lock(&joydev->mutex);
        if(joydev->exist && !--joydev->open)
            input_close_device(&joydev->handle);
        mutex_unlock(&joydev->mutex);
	}
/*list_for_each_entry该宏类似于list_for_each_rcu，不同之处在于它用于遍历指定类型的数据结
构链表，当前链表项pos为一包含struct list_head结构的特定的数据结构。
kill_fasync当执行 kill_fasync() 时，将向该设备 async_queue 链表中的所有使用 fasync_help() 
登记的进程发送信号，注意这里的从入口得到的 sig（一般为SIGIO）只作为不是 SIGURG 的指示，真
正向进程发送的信号将为f_owner.signum。*/
/*将设备和client占用的内存释放掉。*/
static int joydev_release(struct inode *inode ,struct file *file){
     struct joydev_client *client =file->private_data;
     struct joydev *joydev = client->joydev;
    /*将设备和client分离，然后各自注销。*/
     joydev_detach_client(joydev , client);
     kfree(client);

     joydev_close_device(joydev);
      /*注销设备*/
     put_device(&joydev->dev);
     return 0;

}
/*先查看在connect()函数中创建的节点文件的次设备号
  如大于驱动支持的最大设备数JOYDEV_MINORS则，错误，返回
  否则，将驱动支持的joydev设备上锁，如果有错，就返回错误
  否则，将joydev_table[minor]，即，节点文件对应的次设备的对应设备
  传给一个joydev,如果有错就返回。否则，为一个client结构体，分配内存
  若错，就返回。否则初始化client的中断锁，就是锁住存放event结构体的数组
  ，并且将client和joydev 结合起来。*/
static int joydev_open(struct inode *inode , struct file *file){
		struct joydev_client *client;
                struct joydev *joydev;
 		int error;
                int i=iminor(inode);
		if(i>JOYDEV_MINORS)
			return -ENODEV;
		error = mutex_lock_interruptible(&joydev_table_mutex);
                if(error)
                     return error;
                joydev = joydev_table[i];
                if(joydev)
			get_device(&joydev->dev);
                mutex_unlock(&joydev_table_mutex);
		
		if(!joydev)return -ENODEV;

		client  = kzalloc(sizeof(struct joydev_client),GFP_KERNEL);
		if(!client){
		   error = -ENOMEM;
                   put_device(&joydev->dev);
	           return error;
		}
                spin_lock_init(&client->buffer_lock);
		client->joydev = joydev;
                joydev_attach_client(joydev,client);
		error  = joydev_open_device(joydev);
		if(error)
	         {
			joydev_detach_client(joydev, client);
	                kfree(client);
                        return error;
		}
               file->private_data = client;
		nonseekable_open(inode , file);
            return 0;
               
}
/*将event复制到用户内存空间。*/
static ssize_t joydev_read(struct file *file, char __user *buf,
			   size_t count, loff_t *ppos)
{
	struct joydev_client *client = file->private_data;
	struct joydev *joydev = client->joydev;
	struct input_dev *input = joydev->handle.dev;
	struct js_event event;
         //event=client->buffer[1];
 event=client->buffer[client->head] ;
int i=0;
        
        if (copy_to_user(buf, &event, sizeof(struct js_event)))   
			return -EFAULT;

    
}

static const struct file_operations joydev_fops = {
			.owner   = THIS_MODULE,
			.read    = joydev_read,
			.open    = joydev_open,
			.release = joydev_release,
};

/*先获得一个可用的次设备号，没有就返回错误。然后对joydev的成员各种初始化。
  对joydev中的handle各种初始化，然后。调用input_register_handle()函数将handle
  对应的设备和对应的中断处理handler结合起来。*/
static int joydev_connect(struct input_handler *handler, struct input_dev *dev,
			  const struct input_device_id *id)
{
	struct joydev *joydev;
	int i, j, t, minor;
	int error;
          printk(KERN_ERR "joydev connect:%d:%d\n",INPUT_MAJOR,JOYDEV_MINOR_BASE + 
minor);/*测试用语句*/
	for (minor = 0; minor < JOYDEV_MINORS; minor++)
		if (!joydev_table[minor]) /*找到一个空的编号，刚刚连上来的那个设备就用这
个编号*/
			break;

	if (minor == JOYDEV_MINORS) {
		printk(KERN_ERR "joydev: no more free joydev devices\n");
		return -ENFILE;
	}

	joydev = kzalloc(sizeof(struct joydev), GFP_KERNEL);
/*给设备分配内存,用kzalloc申请内存的时候， 效果等同于先是用 kmalloc() 申请空间 , 然后用 
memset() 来初始化 ,所有申请的元素都被初始化为 0.kmalloc特殊之处在于它分配的内存是物理上连
续的*/
	if (!joydev)
		return -ENOMEM;

	INIT_LIST_HEAD(&joydev->client_list);/*初始化队列，头尾指向同一个地方*/
	spin_lock_init(&joydev->client_lock);/*初始化内核排队自旋锁*/
	mutex_init(&joydev->mutex);/*初始化互斥锁，初始状态为打开*/
	

	dev_set_name(&joydev->dev, "js%d", minor);/*给设备命名,按此设备号递增*/
	joydev->exist = true; /*初始化成员变量*/
	joydev->minor = minor;/*初始化成员变量*/

	joydev->handle.dev = input_get_device(dev);/*从给定的设备指针,获取一个设备*/
	joydev->handle.name = dev_name(&joydev->dev);/**/
	joydev->handle.handler = handler;/**/
	joydev->handle.private = joydev;/**/
           joydev->nabs=0;
	for (i = 0; i < ABS_CNT; i++)/*测试设备中 控制有多少个方向的的值absbit，哪一位为1 
就表示支持，就将nkey传给 absmap，abspam中存的是支持的方向是第几位。这2个数组互相映射。*/
		if (test_bit(i, dev->absbit)) { /*测试第i位是否为1*/
			joydev->absmap[i] = joydev->nabs;
			joydev->abspam[joydev->nabs] = i;
			joydev->nabs++;
		}
   for (i = 32; i < 43; i++)
	//for (i = BTN_JOYSTICK - BTN_MISC; i < KEY_MAX - BTN_MISC + 1; i++)
/* BIN_MISC  
，事件类型如果和别的输入类型都不匹配，那么就是BIN_MISC型*/
		if (test_bit(i + BTN_MISC, dev->keybit)) {  /**/
			joydev->keymap[i] = joydev->nkey;
			joydev->keypam[joydev->nkey] = i + BTN_MISC;
			joydev->nkey++;
		}

	

	for (i = 0; i < joydev->nabs; i++) { /*有多少个方向*/
		j = joydev->abspam[i];/*第j个方向*/
		if (input_abs_get_max(dev, j) == input_abs_get_min(dev, j)) {
			joydev->corr[i].type = JS_CORR_NONE; /*JS_CORR_NONE的值为：0;*/
			joydev->abs[i] = input_abs_get_val(dev, j);
			continue;
		}
		joydev->corr[i].type = JS_CORR_BROKEN;/*JS_CORR_BROKEN的值为：1;*/
		joydev->corr[i].prec = input_abs_get_fuzz(dev, j);

		t = (input_abs_get_max(dev, j) + input_abs_get_min(dev, j)) / 2;
		joydev->corr[i].coef[0] = t - input_abs_get_flat(dev, j);
		joydev->corr[i].coef[1] = t + input_abs_get_flat(dev, j);

		t = (input_abs_get_max(dev, j) - input_abs_get_min(dev, j)) / 2
			- 2 * input_abs_get_flat(dev, j);
		if (t) {
			joydev->corr[i].coef[2] = (1 << 29) / t;
			joydev->corr[i].coef[3] = (1 << 29) / t;

			joydev->abs[i] =
				joydev_correct(input_abs_get_val(dev, j),
					       joydev->corr + i);
		}
	}

	joydev->dev.devt = MKDEV(INPUT_MAJOR, JOYDEV_MINOR_BASE + minor);
	joydev->dev.class = &input_class;/*加入input设备链表*/
	joydev->dev.parent = &dev->dev;
	joydev->dev.release = joydev_free;
	device_initialize(&joydev->dev);/*这里所做的都是初始化一个设备*/

	
/*在这个函数里所做的处理其实很简
单。将handle挂到所对应input device的h_list链表上。还将handle挂到对应的handler的hlist链表
上。如果handler定义了start函数，将调用之。*/
	  error = input_register_handle(&joydev->handle);
           if(error){put_device(&joydev->dev);return error;}
                joydev_table[joydev->minor] = joydev;
             error = device_add(&joydev->dev);
           if(error){
       

	
       mutex_lock(&joydev->mutex);
	joydev->exist = false;
	mutex_unlock(&joydev->mutex);
	
       mutex_lock(&joydev_table_mutex);
	joydev_table[joydev->minor] = NULL;
	mutex_unlock(&joydev_table_mutex);



         return error;
}

	return 0;
}

/*先通过handle获取这个设备，然后进行各种注销
  因为设备和handler是通过handle结合起来，一起工作的，
  所以注销传递的是handle。*/
static void joydev_disconnect(struct input_handle *handle)
{
	/*通过handle，获得包含它的结构体*/
     struct joydev *joydev = handle->private;
       printk("you are a 太棒了\n");
       /*删除内存中实际存在的设备joydev->dev，包括设备节点文件等*/
	device_del(&joydev->dev);
	
	/*改变设备joydev中exist的值，表示这个设备已经不存在了*/
       mutex_lock(&joydev->mutex);
	joydev->exist = false;
	mutex_unlock(&joydev->mutex);
        
       /*将在这个驱动中同时开启的设备中，将这个设备的去掉，不再存储。*/
        mutex_lock(&joydev_table_mutex);
	joydev_table[joydev->minor] = NULL;
	mutex_unlock(&joydev_table_mutex);

	
	/*注销掉handle*/
	input_unregister_handle(handle);
        /*注销掉设备*/
	put_device(&joydev->dev);
}


static const struct input_device_id joydev_ids[]={
	{
	  .flags =INPUT_DEVICE_ID_MATCH_EVBIT |
 		INPUT_DEVICE_ID_MATCH_KEYBIT,
          .evbit = {BIT_MASK(EV_KEY)},
	  .keybit = {[BIT_WORD(BTN_JOYSTICK)]=BIT_MASK(BTN_JOYSTICK)} ,
        },
{}      /*表示结束*/
};

MODULE_DEVICE_TABLE(input , joydev_ids);/*该语句创建一个名为__mod_input_device_table的局  
                                    部变量指向struct input_device_id数组，即          
                                joydev_ids[].在稍后的内核构建中depmod
                                          程序在所有的模块中搜索符号                      
                    __mod_input_device_table,如果找到了该符号，就
                                          把它添加到文                                    
                                          件/lib/modules/KERNEL_VERSION/module.inputmap中
                                        。当depmod结束之后，内核模块支持的所有input设备
                                       连同他们的模块名都在该文件中列出。当内核告知系统一 
                                     个新的input设备已经被发现时，热插拔系统使用          
                             modules.pcimod文件来寻找要装载的恰当驱动程序*/

static struct input_handler joydev_handler={
	.event	    =	joydev_event,
	.connect    =	joydev_connect,
	.disconnect =	joydev_disconnect,
        .fops	    =   &joydev_fops,
	.minor      =   JOYDEV_MINOR_BASE,
        .name       =   "joydev",
	.id_table   =   joydev_ids,
           };
static int __init joydev_init(void){
	return input_register_handler(&joydev_handler);
}
static void __exit joydev_exit(void){
	input_unregister_handler(&joydev_handler);
}
module_init(joydev_init);
module_exit(joydev_exit);
