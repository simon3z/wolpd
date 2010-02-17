#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef DEFAULT_IFACE
#define DEFAULT_IFACE "eth0"
#endif

#ifndef DEFAULT_PORT
#define DEFAULT_PORT 9
#endif

#define WOL_BUFSIZE     1024


int       g_foreground  = 0;
char     *g_interface   = DEFAULT_IFACE;
uint16_t  g_port        = DEFAULT_PORT;
uint8_t   g_magic[]     = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
uint8_t   g_woltype[]   = { 0x08, 0x42 };


void version_and_exit()
{
    printf("\
%s\n\n\
Copyright (C) 2010 Federico Simoncelli\n\
License GPLv3+: \
GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.\n\n\
Written by Federico Simoncelli.\n",
        PACKAGE_STRING);

    exit(EXIT_SUCCESS);
}

void usage_and_exit()
{
    printf("\
%s is a Wake-On-Lan proxy daemon.\n\n\
Usage: %s [OPTION]...\n\n\
Options:\n\
  -h, --help              print this help, then exit.\n\
  -v, --version           print version number, then exit.\n\
  -f, --foreground        don't fork to background.\n\
  -i, --interface=IFACE   destination network interface (default: %s).\n\
  -p, --port=PORT         udp port used for wol packets (default: %i).\n\n\
Report bugs to <%s>.\n",
        PACKAGE_NAME, PACKAGE_NAME,
        DEFAULT_IFACE, DEFAULT_PORT, PACKAGE_BUGREPORT);
    exit(EXIT_SUCCESS);
}

void parse_options(int argc, char *argv[])
{
    int c;

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"help", 0, 0, 'h'},
            {"version", 0, 0, 'v'},
            {"foreground", 0, 0, 'f'},
            {"interface", 1, 0, 'i'},
            {"port", 1, 0, 'p'},
            {NULL, 0, NULL, 0}
        };

        if ((c = getopt_long(argc, argv, "hvi:f",
                     long_options, &option_index)) == -1) break;

        switch (c) {
            case 'h':
                usage_and_exit();
                break;
            case 'v':
                version_and_exit();
                break;
            case 'i':
                g_interface = optarg;
                break;
            case 'f':
                g_foreground = 1;
                break;
        }
    }
}

void background(void) {
    pid_t pid, sid;
    
    if ((pid = fork()) < 0) {
        exit(EXIT_FAILURE);
    }
    else if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    umask(0);

    if ((sid = setsid()) < 0) {
        exit(EXIT_FAILURE);
    }
    
    if ((chdir("/")) < 0) {
        exit(EXIT_FAILURE);
    }
    
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

int main(int argc, char *argv[])
{
    int ex_socket, in_socket, sockbcast;
    uint8_t wol_buf[WOL_BUFSIZE], *wol_msg, *wol_hw;
    ssize_t wol_len;
    struct ifreq ifhw, ifid;
    struct sockaddr_in wol_src, wol_rmt;
    struct sockaddr_ll wol_dst;
    socklen_t wol_rmt_len;

    parse_options(argc, argv);

    /* initializing */
    strncpy(ifhw.ifr_name, g_interface, sizeof(ifhw.ifr_name));
    strncpy(ifid.ifr_name, g_interface, sizeof(ifid.ifr_name));

    sockbcast = 1;
    wol_msg   = wol_buf + ETHER_HDR_LEN;

    if ((ex_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("couldn't open external socket");
        goto exit_fail1;
    }

    if ((in_socket = socket(PF_PACKET, SOCK_RAW, 0)) < 0 ) {
        perror("couldn't open internal socket");
        goto exit_fail2;
    }

    if (ioctl(in_socket, SIOCGIFHWADDR, &ifhw) < 0) {
        perror("couldn't request local hwaddress");
        goto exit_fail3;
    }

    if (ioctl(in_socket, SIOCGIFINDEX, &ifid) < 0) {
        perror("couldn't request adapter index");
        goto exit_fail3;
    }

    if (setsockopt(in_socket, SOL_SOCKET,
            SO_BROADCAST, &sockbcast, sizeof(sockbcast)) < 0) {
        perror("couldn't use broadcast socket");
        goto exit_fail3;
    }

    memset(&wol_src, 0, sizeof(wol_src));
    wol_src.sin_family      = AF_INET;
    wol_src.sin_addr.s_addr = htonl(INADDR_ANY);
    wol_src.sin_port        = htons(g_port);

    memset(&wol_dst, 0, sizeof(wol_dst));
    wol_dst.sll_family  = AF_PACKET;
    wol_dst.sll_ifindex = ifid.ifr_ifindex;
    wol_dst.sll_halen   = ETH_ALEN;

    if (bind(ex_socket, (struct sockaddr *) &wol_src, sizeof(wol_src)) < 0) {
        perror("couldn't bind to local interface");
        goto exit_fail3;
    }

    /* initializing wol message */
    memcpy(wol_buf + (ETHER_ADDR_LEN), ifhw.ifr_hwaddr.sa_data, ETHER_ADDR_LEN);
    memcpy(wol_buf + (ETHER_ADDR_LEN * 2), g_woltype, ETHER_TYPE_LEN);

    if (g_foreground == 0) background();

    while (1)
    {
        wol_rmt_len = sizeof(wol_rmt);

        if ((wol_len = recvfrom(
                ex_socket, wol_msg, WOL_BUFSIZE - ETHER_HDR_LEN, 0,
                    (struct sockaddr *) &wol_rmt, &wol_rmt_len)) < 0) {
            perror("couldn't receive data from external socket");
            goto exit_fail3;
        }

        if (memcmp(wol_msg, g_magic, sizeof(g_magic)) != 0) {
            syslog(LOG_ERR,
                "unknown packed from %s", inet_ntoa(wol_rmt.sin_addr));
            continue;
        }

        wol_hw = wol_msg + sizeof(g_magic);

        syslog(LOG_NOTICE, "forwarding magic packet from %s to "
            "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
            inet_ntoa(wol_rmt.sin_addr),
            wol_hw[0], wol_hw[1], wol_hw[2],
            wol_hw[3], wol_hw[4], wol_hw[5]
        );
        
        memcpy(wol_buf, wol_hw, ETH_ALEN);
        memcpy(wol_dst.sll_addr, wol_hw, ETH_ALEN);

        if ((wol_len = sendto(
                in_socket, wol_buf, (size_t) wol_len + ETHER_HDR_LEN, 0,
                    (struct sockaddr *) &wol_dst, sizeof(wol_dst))) < 0) {
            perror("couldn't send data to internal socket");
            goto exit_fail3;
        }
    }

exit_fail3:
    close(in_socket);

exit_fail2:
    close(ex_socket);

exit_fail1:
    return EXIT_FAILURE;
}

