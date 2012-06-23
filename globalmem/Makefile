#Makefile脚本，用于编译globalmem.c模块驱动文件
#编译成模块
obj-m := globalmem.o 

#编译器路径
KERNELBUILD := /lib/modules/`uname -r`/build

#默认编译为模块
default:
	 make -C $(KERNELBUILD) M=$(shell pwd) modules

#清空
clean:
	 rm -rf *.o .*.cmd *.ko *.mod.c .tmp_versions *.symvers *.order
