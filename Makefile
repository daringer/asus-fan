KVERSION ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KVERSION)/build

CFLAGS_asus_fan.o += -Wno-format -Wno-format-extra-args

ifeq ($(DEBUG), 1)
    CFLAGS_asus_fan.o += -DDEBUG 
endif


obj-m := asus_fan.o mach_base.o mach_default.o mach_ux410uak.o machines.o

all:
	make -C $(KDIR) M=$$PWD modules

install:
	make -C $(KDIR) M=$$PWD modules_install
	depmod -a

clean:
	make -C $(KDIR) M=$$PWD clean

removeloadmodule: all
	sudo rmmod asus-fan || true
	sudo insmod asus_fan.ko
	sudo systemctl restart asus-fan.service
