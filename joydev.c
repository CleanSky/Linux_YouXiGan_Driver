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
MODULE_LICENSE("GPL"); /*内核模块使用的许可证*/


#define JOYDEV_MINOR_BASE 0
#define JOYDEV_MINORS 16
#define JOYDEV_BUFFER_SIZE 64  //最多存取的中断数，即event的数目。

struct joydev {
    int open; /*记录设备文件被打开的次数*/
    int minor; /*给连接的设备分配的次设备号*/
    struct input_handle handle; /* 结构体handle中有一个
                                 设备结构体和一个handler结构体
                                  中断发生时，会先查找设备中的handle_list中
                                 的第一个激活的 handle,然后再根据handle
                                 再找到中断处理器handler。所以handle结构体中
                                 会有一个设备dev结构体，和一个handler结构体。
                                 handle将dev和handler结合了起来。*/

    struct list_head client_list;
    spinlock_t client_lock; /*设备链表自旋锁,用来在单处理器中屏蔽可抢占，主要保证锁中内容是原子操作。*/
    struct mutex mutex; /*设备互斥锁*/
    struct device dev; /*设备结构体*/

    bool exist; /*设备是否断开*/
    int nabs; /*方位数*/
    int nkey; /*按键数*/
    /*BTN_MISC=0X100
      KEY_MAX=0x2ff
      ABS_CNT=0x40
      BTN_STICK=0x120*/
    __u16 keymap[KEY_MAX - BTN_MISC + 1];
    __u8 absmap[ABS_CNT];
};


struct joydev_client { /*存放中断。*/
    struct js_event buffer[JOYDEV_BUFFER_SIZE]; /*中断数*/
    int head;
    int startup; /*发生中断数*/
    spinlock_t buffer_lock; /** 保护buffer */
    struct joydev *joydev; /*设备，主要用来显示中断数的多少*/
    struct list_head node; /*用于RCU控制，*/
};
static struct joydev *joydev_table[JOYDEV_MINORS]; /*驱动支持的设备数组*/
static DEFINE_MUTEX(joydev_table_mutex); /*锁*/

/*先查看在connect()函数中创建的节点文件的次设备号
  如大于驱动支持的最大设备数JOYDEV_MINORS则，错误，返回
  否则，将驱动支持的joydev设备上锁，如果有错，就返回错误
  否则，将joydev_table[minor]，即，节点文件对应的次设备的对应设备
  传给一个joydev,如果有错就返回。否则，为一个client结构体，分配内存
  若错，就返回。否则初始化client的中断锁，就是锁住存放event结构体的数组
  ，并且将client和joydev 结合起来。*/
static int joydev_open(struct inode *inode, struct file *file) {
    struct joydev_client *client;
    struct joydev *joydev;
    int error;
    int i = iminor(inode);
    if (i > JOYDEV_MINORS)
        return -1;

    mutex_lock(&joydev_table_mutex);
    joydev = joydev_table[i];
    mutex_unlock(&joydev_table_mutex);

    if (!joydev)return -ENODEV;

    client = kzalloc(sizeof (struct joydev_client), GFP_KERNEL);
    if (!client) {
        error = -2;
        put_device(&joydev->dev);
        return error;
    }
    spin_lock_init(&client->buffer_lock);
    client->joydev = joydev;
    spin_lock(&joydev->client_lock);
    /*static inline void list_add_rcu(struct list_head *new, struct list_head *head) 
    该函数把链表项new插入到RCU保护的链表head的开头。使用内存栅保证了在引用这个新插入的链表项
    之前，新链表项的链接指针的修改对所有读者是可见的。*/
    list_add_tail_rcu(&client->node, &joydev->client_list);
    spin_unlock(&joydev->client_lock);


    mutex_lock(&joydev->mutex);
    if (!joydev->exist)error = -3;
    else if (!joydev->open++) {
        /*如果是第一次打开设备，就调用input_open_device()
          主要还是打开handle中对应的设备。*/
        error = input_open_device(&joydev->handle);
        if (error)joydev->open--;
    }
    mutex_unlock(&joydev->mutex);
    if (error) {

        spin_lock(&joydev->client_lock);
        list_del_rcu(&client->node);
        spin_unlock(&joydev->client_lock);
        kfree(client);
        return -4;
    }
    file->private_data = client;

    return 0;
}

/*将event复制到用户内存空间。*/
static ssize_t joydev_read(struct file *file, char __user *buf,
        size_t count, loff_t *ppos) {
    struct joydev_client *client = file->private_data;

    struct js_event event;
    //event=client->buffer[1];
    spin_lock(&client->buffer_lock);
    event = client->buffer[client->head];
    spin_unlock(&client->buffer_lock);


    if (copy_to_user(buf, &event, sizeof (struct js_event)))
        return -EFAULT;
}

static int joydev_release(struct inode *inode, struct file *file) {
    struct joydev_client *client = file->private_data;
    struct joydev *joydev = client->joydev;
    /*将设备和client分离，然后各自注销。*/

    spin_lock(&joydev->client_lock);
    list_del_rcu(&client->node);
    spin_unlock(&joydev->client_lock);

    kfree(client);

    mutex_lock(&joydev->mutex);
    if (joydev->exist && !--joydev->open)
        input_close_device(&joydev->handle);
    mutex_unlock(&joydev->mutex);
    /*注销设备*/
    put_device(&joydev->dev);
    return 0;
}

static const struct file_operations joydev_fops = {
    .owner = THIS_MODULE,
    .read = joydev_read,
    .open = joydev_open,
    .release = joydev_release,
};

/*先获得一个可用的次设备号，没有就返回错误。然后对joydev的成员各种初始化。
  对joydev中的handle各种初始化，然后。调用input_register_handle()函数将handle
  对应的设备和对应的中断处理handler结合起来。*/
static int joydev_connect(struct input_handler *handler, struct input_dev *dev,
        const struct input_device_id *id) {
    struct joydev *joydev;
    int i, j, t, minor;
    int error;
    printk(KERN_ERR "joydev connect:%d:%d\n", INPUT_MAJOR, JOYDEV_MINOR_BASE +
            minor); /*测试用语句*/
    for (minor = 0; minor < JOYDEV_MINORS; minor++)
        if (!joydev_table[minor]) /*找到一个空的编号，刚刚连上来的那个设备就用这
个编号*/
            break;

    if (minor == JOYDEV_MINORS) {
        printk(KERN_ERR "joydev: no more free joydev devices\n");
        return -ENFILE;
    }

    joydev = kzalloc(sizeof (struct joydev), GFP_KERNEL);
    /*给设备分配内存,用kzalloc申请内存的时候， 效果等同于先是用 kmalloc() 申请空间 , 然后用 
    memset() 来初始化 ,所有申请的元素都被初始化为 0.kmalloc特殊之处在于它分配的内存是物理上连
    续的*/
    if (!joydev)
        return -ENOMEM;

    INIT_LIST_HEAD(&joydev->client_list); /*初始化队列，头尾指向同一个地方*/
    spin_lock_init(&joydev->client_lock); /*初始化内核排队自旋锁*/
    mutex_init(&joydev->mutex); /*初始化互斥锁，初始状态为打开*/


    dev_set_name(&joydev->dev, "js%d", minor); /*给设备命名,按此设备号递增*/
    joydev->exist = true; /*初始化成员变量*/
    joydev->minor = minor; /*初始化成员变量*/

    joydev->handle.dev = input_get_device(dev); /*从给定的设备指针,获取一个设备*/
    joydev->handle.name = dev_name(&joydev->dev); /**/
    joydev->handle.handler = handler; /**/
    joydev->handle.private = joydev; /**/
    joydev->nabs = 0;
    for (i = 0; i < ABS_CNT; i++)/*测试设备中 控制有多少个方向的的值absbit，哪一位为1 
就表示支持，就将nkey传给 absmap，abspam中存的是支持的方向是第几位。这2个数组互相映射。*/
        if (test_bit(i, dev->absbit)) { /*测试第i位是否为1*/
            joydev->absmap[i] = joydev->nabs;
            joydev->nabs++;
        }

    for (i = BTN_JOYSTICK - BTN_MISC; i < KEY_MAX - BTN_MISC + 1; i++)
        /* BIN_MISC  
        ，事件类型如果和别的输入类型都不匹配，那么就是BIN_MISC型*/
        if (test_bit(i + BTN_MISC, dev->keybit)) { /**/
            joydev->keymap[i] = joydev->nkey;
            joydev->nkey++;
        }

    joydev->dev.devt = MKDEV(INPUT_MAJOR, JOYDEV_MINOR_BASE + minor);
    joydev->dev.class = &input_class; /*加入input设备链表*/
    joydev->dev.parent = &dev->dev;   /*指向其父设备*/
    device_initialize(&joydev->dev); /*这里所做的都是初始化一个设备
         ,用了这个函数后，可以很方便的使用get——device  put——device()函数。*/


    /*在这个函数里所做的处理其实很简
    单。将handle挂到所对应input device的h_list链表上。还将handle挂到对应的handler的hlist链表
    上。如果handler定义了start函数，将调用之。*/
    error = input_register_handle(&joydev->handle);
    if (error) {
        put_device(&joydev->dev);
        return error;
    }
    joydev_table[joydev->minor] = joydev;
    error = device_add(&joydev->dev);
    if (error) {

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

static void joydev_pass_event(struct joydev_client *client, struct js_event *event) {
    struct joydev *joydev = client->joydev;

    spin_lock(&client->buffer_lock); /*上锁*/

    client->buffer[client->head] = *event;
    if (client->startup == joydev->nabs + joydev->nkey) {
        client->head++;
        client->head &= JOYDEV_BUFFER_SIZE - 1; /*0~63*/
        if (client->head == 63)
            client->startup = 0;
    }
    spin_unlock(&client->buffer_lock);


}

static void joydev_event(struct input_handle *handle, unsigned int type, unsigned int
        code, int value) {
    struct joydev *joydev = handle->private; /*将struct private中的数据传给joydev。*/
    struct joydev_client *client;
    struct js_event event;
    switch (type) {
        case EV_KEY:
            /*value的值只可能为0,1,0表示按下，1表示跳起*/
            if (code < BTN_MISC || value > 1)return;
            event.type = JS_EVENT_BUTTON; /*JS_EVENT_BUTTON=1*/
            event.number = joydev->keymap[code - BTN_MISC];
            event.value = value;
            break;
        case EV_ABS:
            event.type = JS_EVENT_AXIS; /*JS_EVENT_AXIS=2*/
            event.number = joydev->absmap[code];
            event.value = value;

            break;
        default:
            return;
    }
    event.time = jiffies_to_msecs(jiffies); /*当前系统时间*/
    /*if(event.type ==2 && (event.number == 0 || event.number == 1 || event.number==4 || event.number ==3))
     printk("444time:%u,value:%x,type:%x,number:%x\n",event.time,event.value,event.type,event.number);*/
    int flag = 0;
    if (event.type == 1 && event.value == 1) {
        switch (event.number) {
            case 0: printk("press 1\n");
                break;
            case 1: printk("press 2\n");
                break;
            case 2: printk("press 3\n");
                break;
            case 3: printk("press 4\n");
                break;
            case 4: printk("press left 1\n");
                break;
            case 5: printk("press right 1\n");
                break;
            case 6: printk("press left 2\n");
                break;
            case 7: printk("press right 2\n");
                break;
            case 8: printk("press select \n");
                break;
            case 9: printk("press start\n");
                break;
            default: flag = 1;
        }
        if (flag == 0) {

            rcu_read_lock();
            list_for_each_entry_rcu(client, &joydev->client_list, node)

            joydev_pass_event(client, &event);

            rcu_read_unlock();
        }
        flag = 0;
    }
    else if (event.type == 2) {
        switch (event.value) {
            case 0xff: if (event.number == 0)printk("press right\n");
                else if (event.number == 1)printk("press down\n");
                rcu_read_lock();
                list_for_each_entry_rcu(client, &joydev->client_list, node)
                joydev_pass_event(client, &event);
                rcu_read_unlock();
                break;

            case 0x00: if (event.number == 0)printk("press left\n");
                else if (event.number == 1)printk("press up\n");
                rcu_read_lock();
                list_for_each_entry_rcu(client, &joydev->client_list, node)
                joydev_pass_event(client, &event);
                rcu_read_unlock();
                break;
            default:;
        }
    }
}

/*先通过handle获取这个设备，然后进行各种注销
  因为设备和handler是通过handle结合起来，一起工作的，
  所以注销传递的是handle。*/
static void joydev_disconnect(struct input_handle *handle) {
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


static const struct input_device_id joydev_ids[] = {
    {
        .flags = INPUT_DEVICE_ID_MATCH_EVBIT |
        INPUT_DEVICE_ID_MATCH_KEYBIT,
        .evbit =
        {BIT_MASK(EV_KEY)},
        .keybit =
        {[BIT_WORD(BTN_JOYSTICK)] = BIT_MASK(BTN_JOYSTICK)},
    },
    {} /*表示结束*/
};

MODULE_DEVICE_TABLE(input, joydev_ids); /*该语句创建一个名为__mod_input_device_table的局  
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

static struct input_handler joydev_handler = {
    .event = joydev_event,
    .connect = joydev_connect,
    .disconnect = joydev_disconnect,
    .fops = &joydev_fops,
    .minor = JOYDEV_MINOR_BASE,
    .name = "joydev",
    .id_table = joydev_ids,
};

static int __init joydev_init(void) {
    return input_register_handler(&joydev_handler);
}

static void __exit joydev_exit(void) {
    input_unregister_handler(&joydev_handler);
}
module_init(joydev_init);
module_exit(joydev_exit);
