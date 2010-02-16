[NAME]
wolpd \- Wake-On-Lan proxy daemon

[DESCRIPTION]
Wake-on-LAN is an Ethernet computer networking standard that allows a computer
to be turned on or woken up by a network message. The message is usually sent
by a simple program executed on another computer on the local area network.
In order to use Wake-on-LAN over the Internet the appropriate message (magic
packet) needs to be forwarded from the WAN side to the LAN side of the gateway.
Wolpd is a Wake-on-LAN proxy daemon designed to analyze, log and eventually
forward the received magic packets to the LAN hosts.

[SEE ALSO]
ether-wake(8)
