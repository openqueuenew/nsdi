# README #

This README includes steps necessary to get OpenQueue implementation up and running.

### Build ###

* Install Linux kernel headers (for Debian). This needs to be done only once.

```
# apt-get install build-essential linux-headers-$(uname -r)
``` 

* Build modules

```
# make
```

* Clean modules

```
# make clean
```


### Install Modules ###

* Install OpenQueue Qdisc

```
# sudo insmod qdisc/sch_openqueue.ko
```

* Install OpenQueue Policy oq_mod

```
# sudo insmod policy/oq_mod.ko
```

* Install OpenQueue Policy oq_mod_2

```
# sudo insmod policy/oq_mod_2.ko
```

* Check status

```
# lsmod | grep openqueue
oq_mod      16384 0
oq_mod_2    16384 0
sch_openqueue 16384 2 oq_mod, oq_mod_2
```


### Attach Qdisc ###
Please note that you need to make sure you use the extended version of "tc" available in this repo. 

* Attach OpenQueue qdisc with Policy oq_mod to interface eth0

```
# sudo tc qdisc add dev eth0 root openqueue policy oq_mod
```

* Check status of interface eth0 after attaching our qdisc

```
# tc qdisc show dev eth0
qdisc openqueue 8001: root refcnt 2 policy oq_mod
```
