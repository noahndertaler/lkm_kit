KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

obj-m := hello_lkm.o

all:
	make -C $(KDIR) M=$(PWD) modules
	sudo insmod hello_lkm.ko filename=bla.py program=python user=noah protocol=/proc/net/raw fdNum=5 kernel_version=$(shell uname -r)	
	# Load module at boot time:
	# echo "hello_lkm" >> /etc/modules
clean:
	make -C $(KDIR) M=$(PWD) clean
	sudo rmmod hello_lkm.ko
