ifneq (${KERNELRELEASE},)
	obj-m += fpga-dma.o
else
#set KDIR to kernel source root
#set BUILD_DIR to desired build directory
	KDIR := /usr/src/linux-headers-$(shell uname -r)
	BUILD_DIR := $(shell pwd)
        #LDFLAGS=-pthread

#default: module dma_proxy_test 

module:
	$(MAKE) SUBDIRS=$(BUILD_DIR) -C $(KDIR) modules
clean:
	$(MAKE) SUBDIRS=$(BUILD_DIR) -C $(KDIR) clean
	${RM} hello
endif
