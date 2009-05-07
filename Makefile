obj-$(CONFIG_HAMMER_FS) += hammer.o

hammer-objs := file.o super.o dfly_wrap.o hammer_ondisk.o

ifndef EXTRA_CFLAGS
	export EXTRA_CFLAGS = -I$(shell pwd)/fs/hammerfs/dfly
endif

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	rm -f .*.cmd *.o .*.o.d modules.order
#	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
