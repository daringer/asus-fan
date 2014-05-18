obj-m := asus_fan.o
KDIR := /lib/modules/$(shell  uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

install:
	# just copy the .ko file anywhere below:
	# /lib/modules/$(uname -r)/

