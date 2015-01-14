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

```sh
  $ ./autogen.sh
  $ ./configure
  $ make
```

### Building the tarball package:

```sh
  $ make dist-gzip
```

### Building the rpms:

```sh
  $ make rpmbuild
```

Install Instructions
====================

```sh
  # make install
```

Debugging Tips
==============

```sh
  # tcpdump -i <interface> ether proto 0x0842
```

