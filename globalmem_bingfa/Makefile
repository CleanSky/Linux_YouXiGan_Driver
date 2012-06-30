obj-m := globalmem.o

default:
	make -C /lib/modules/`uname -r`/build M=$(shell pwd) modules

clean:
	rm -rf *.o *.ko *.order *.symvers *.out *.mod.c

distclean:
	rm -rf *.o *.ko *.order *.symvers *.out *.mod.c .*.cmd *~ .tmp_versions
