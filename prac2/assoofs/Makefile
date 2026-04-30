obj-m := assoofs.o
all: ko mkassoofs
ko:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules
mkassoofs: mkassoofs.c assoofs.h
	gcc -o mkassoofs mkassoofs.c
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean
	rm -f mkassoofs
