# Short instruction for building kernel module

ifneq ($(KERNELRELEASE),)
# call from kernel build system

obj-m	:= shofer.o

else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD       := $(shell pwd)

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
	gcc ./tester.c -o tester

endif

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	rm ./tester
