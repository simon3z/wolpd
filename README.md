## General Instructions

Copyright (C) 2015 Charles-Antoine Degennes <cadegenn@univ-lr.fr>  
Copyright (C) 2010 Federico Simoncelli <federico.simoncelli@gmail.com>

This file is free documentation; you have unlimited permission to copy,
distribute and modify it.

## Wake-on-LAN Proxy Daemon

Wake-on-LAN is an Ethernet computer networking standard that allows a computer
to be turned on or woken up by a network message. The message is usually sent
by a simple program executed on another computer on the local area network.
In order to use Wake-on-LAN accross routers, the appropriate message (magic
packet) needs to be forwarded from one subnet to another of the gateway.  
Wolpd is a Wake-on-LAN proxy daemon designed to analyze, log and eventually
forward the received magic packets to appropriate subnet.

## Build Instructions

### Building from the repository (requires autoreconf):

```console
$ ./autogen.sh
$ ./configure
$ make
```

### Building the tarball package:

```console
$ make dist-gzip
```

### Building the rpms:

```console
$ make rpmbuild
```

## Install Instructions

```console
# make install
```

## How it works ?

*wolpd* listen on a network interface for incoming wol magic packet.
It then try to find on wich outgoing interface the destination machine is.
To find it, *wolpd* reads configuration files in `/etc` named after each 
network interface :
```console
/etc/wolpd.eth0
/etc/wolpd.eth1
/etc/wolpd.eth2
...
```
and so on.
Each of these files contain one hardware address per line of the form
```console
xx:xx:xx:xx:xx:xx
```

If it do not find the destination wol mac address of the magic packet in any 
configuration files, it discard the packet.  
If it find a match in any file, it build another wol magic packet and send it
on the correct interface.

## Use Instructions

```console
# ./wolpd -h
wolpd is a Wake-On-Lan proxy daemon.

Usage: wolpd [OPTION]...

Options:
  -d, --debug             print debug informations.
  -D, --devel             print development informations.
  -h, --help              print this help, then exit.
  -v, --version           print version number, then exit.
  -f, --foreground        don't fork to background.
  -i, --interface=IFACE   source network interface (default: eth0).
  -p, --port=PORT         udp port used for wol packets (default: 9).

Report bugs to <cadegenn@univ-lr.fr>.
```

## Debugging Tips

```console
# tcpdump -i <interface> ether proto 0x0842
```

