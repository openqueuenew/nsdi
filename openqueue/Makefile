obj-m += qdisc/sch_openqueue.o

obj-m += policy/oq_mod.o
obj-m += policy/oq_mod_2.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
