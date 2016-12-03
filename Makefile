KVERSION ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KVERSION)/build

CFLAGS_asus_fan.o += -Wno-format -Wno-format-extra-args

ifeq ($(DEBUG), 1)
    CFLAGS_asus_fan.o += -DDEBUG 
endif


obj-m := asus_fan.o

all:
	make -C $(KDIR) M=$$PWD modules

install:
	make -C $(KDIR) M=$$PWD modules_install
	depmod -a

clean:
	make -C $(KDIR) M=$$PWD clean
