#Makefile脚本，用来编译helloworld.c模块文件

#编译成模块
obj-m := helloworld.o 

#编译器的路径
KERNELBUILD := /lib/modules/`uname -r`/build

#默认编译成模块
default:
	 make -C $(KERNELBUILD) M=$(shell pwd) modules

#清空处理
clean:
	 rm -rf *.o .*.cmd *.ko *.mod.c .tmp_versions *.order *.symvers *~
