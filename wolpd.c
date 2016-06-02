/* wolpd - Wake-On-LAN Proxy Daemon
 * Copyright (C) 2010  Federico Simoncelli <federico.simoncelli@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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


#define DEFAULT_IFACE "eth0"
#define DEFAULT_PORT  9

#define ETH_P_WOL       0x0842
#define WOL_MAGIC_LEN   6

uint8_t wol_magic[WOL_MAGIC_LEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

struct eth_frame {
    struct ethhdr       head;
    uint8_t             data[ETH_DATA_LEN];
};


/* global options */
char     *g_iface    = DEFAULT_IFACE;
uint16_t  g_port     = DEFAULT_PORT;
int       g_foregnd  = 0;


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
                g_iface = optarg;
                break;
            case 'f':
                g_foregnd = 1;
                break;
        }
    }
}

int main(int argc, char *argv[])
{
    int ex_socket, in_socket;
    struct eth_frame wol_msg;
    ssize_t wol_len;
    struct ifreq ifhw;
    struct sockaddr_in wol_src, wol_rmt;
    struct sockaddr_ll wol_dst;
    socklen_t wol_rmt_len;

    parse_options(argc, argv);

    if ((ex_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("couldn't open external socket");
        goto exit_fail1;
    }

    if ((in_socket = socket(PF_PACKET, SOCK_RAW, 0)) < 0 ) {
        perror("couldn't open internal socket");
        goto exit_fail2;
    }

    /* initializing wol destination */
    strncpy(ifhw.ifr_name, g_iface, sizeof(ifhw.ifr_name));

    if (ioctl(in_socket, SIOCGIFINDEX, &ifhw) < 0) {
        perror("couldn't request adapter index");
        goto exit_fail3;
    }

    memset(&wol_dst, 0, sizeof(wol_dst));
    wol_dst.sll_family  = AF_PACKET;
    wol_dst.sll_ifindex = ifhw.ifr_ifindex;
    wol_dst.sll_halen   = ETH_ALEN;

    /* initializing wol message */
    if (ioctl(in_socket, SIOCGIFHWADDR, &ifhw) < 0) {
        perror("couldn't request local hwaddress");
        goto exit_fail3;
    }

    memcpy(wol_msg.head.h_source, ifhw.ifr_hwaddr.sa_data, ETH_ALEN);
    wol_msg.head.h_proto = htons(ETH_P_WOL);

    memset(&wol_src, 0, sizeof(wol_src));
    wol_src.sin_family      = AF_INET;
    wol_src.sin_addr.s_addr = htonl(INADDR_ANY);
    wol_src.sin_port        = htons(g_port);

    if (bind(ex_socket, (struct sockaddr *) &wol_src, sizeof(wol_src)) < 0) {
        perror("couldn't bind to local interface");
        goto exit_fail3;
    }

    if (g_foregnd == 0) {
        if (daemon(0, 0) != 0) {
            perror("couldn't fork to a background process");
            goto exit_fail3;
        }
    }

    while (1)
    {
        wol_rmt_len = sizeof(wol_rmt);

        if ((wol_len = recvfrom(
                ex_socket, wol_msg.data, ETH_DATA_LEN, 0,
                    (struct sockaddr *) &wol_rmt, &wol_rmt_len)) < 0) {
            perror("couldn't receive data from external socket");
            goto exit_fail3;
        }

        if (wol_len < WOL_MAGIC_LEN + ETH_ALEN) {
            syslog(LOG_ERR,
                "packet too short from %s", inet_ntoa(wol_rmt.sin_addr));
            continue;
        }

        if (memcmp(wol_msg.data, wol_magic, WOL_MAGIC_LEN) != 0) {
            syslog(LOG_ERR,
                "unknown packed from %s", inet_ntoa(wol_rmt.sin_addr));
            continue;
        }

        memcpy(wol_msg.head.h_dest, wol_msg.data + WOL_MAGIC_LEN, ETH_ALEN);
        memcpy(wol_dst.sll_addr, wol_msg.data + WOL_MAGIC_LEN, ETH_ALEN);

        if ((wol_len = sendto(
                in_socket, &wol_msg, (size_t) wol_len + ETH_HLEN, 0,
                    (struct sockaddr *) &wol_dst, sizeof(wol_dst))) < 0) {
            perror("couldn't forward data to internal socket");
            goto exit_fail3;
        }

        syslog(LOG_NOTICE, "magic packet from %s forwarded to "
            "%2.2hhx:%2.2hhx:%2.2hhx:%2.2hhx:%2.2hhx:%2.2hhx",
            inet_ntoa(wol_rmt.sin_addr),
            wol_msg.head.h_dest[0], wol_msg.head.h_dest[1],
            wol_msg.head.h_dest[2], wol_msg.head.h_dest[3],
            wol_msg.head.h_dest[4], wol_msg.head.h_dest[5]
        );
    }

exit_fail3:
    close(in_socket);

exit_fail2:
    close(ex_socket);

exit_fail1:
    return EXIT_FAILURE;
}

