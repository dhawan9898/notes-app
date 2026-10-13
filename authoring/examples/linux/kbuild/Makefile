obj-m += hello.o
KDIR ?= /usr/src/linux-headers-6.12.95+deb13-amd64
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
