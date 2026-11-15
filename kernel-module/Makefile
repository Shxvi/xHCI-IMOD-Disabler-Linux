obj-m += xhci_imod.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

load: default
	sudo insmod xhci_imod.ko

unload:
	sudo rmmod xhci_imod

install: default
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	depmod -a

.PHONY: default clean load unload install
