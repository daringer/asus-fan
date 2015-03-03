KDIR ?= /lib/modules/$(shell uname -r)/build

obj-m := asus_fan.o

all:
	make -C $(KDIR) M=$$PWD modules

install:
	make -C $(KDIR) M=$$PWD modules_install

clean:
	make -C $(KDIR) M=$$PWD clean
