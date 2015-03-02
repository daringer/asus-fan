obj-m := asus_fan.o
KDIR := /lib/modules/$(shell  uname -r)/build

PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

install:
	# just copy the .ko file anywhere below:
	# /lib/modules/$(uname -r)/
	#
	# finally add it to some on-boot-load-mechanism
	# the module will _not_ automatically load.
	cp asus_fan.ko "/lib/modules/$(shell uname -r)/"
	depmod -a
