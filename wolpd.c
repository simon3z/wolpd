#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define BUFFER_SIZE 1024

#ifndef DEFAULT_INTERFACE
#define DEFAULT_INTERFACE "eth0"
#endif

#ifndef DEFAULT_PORT
#define DEFAULT_PORT 9
#endif


int       g_foreground  = 0;
char     *g_interface   = DEFAULT_INTERFACE;
uint16_t  g_port        = DEFAULT_PORT;
uint8_t   g_magic[]     = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };


int ifntoa(char *in, struct in_addr *bcast, long unsigned int t)
{
    int s, e, ret = 0;
    struct ifreq ifr;

    memset(&ifr,0,sizeof(ifr));
    strncpy(ifr.ifr_name, in, sizeof (ifr.ifr_name));

    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        ret = s;
        goto exit_fail1;
    }

    if ((e = ioctl(s, t, &ifr)) < 0) {
        ret = e;
        goto exit_fail2;
    }

    memcpy(bcast,
        &((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr, sizeof(bcast));

exit_fail2:
    close(s);

exit_fail1:
    return ret;
}

int wol_packet(char *p, size_t s, char *hw)
{
    int m;
    char *i;

    for (i = p, m = -1; i <= (p + s - 12); i++) {
        if ((m = memcmp(g_magic, i, sizeof(g_magic))) == 0) break;
    }

    if (m == 0) memcpy(hw, i + 6, 6);

    return m;
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

void version_and_exit()
{
    printf("%s\n\n"
        "Copyright (C) 2010 Federico Simoncelli\n"
        "License GPLv3+: GNU GPL version 3 or later "
        "<http://gnu.org/licenses/gpl.html>.\n"
        "This is free software: you are free to change and redistribute it.\n"
        "There is NO WARRANTY, to the extent permitted by law.\n\n"
        "Written by Federico Simoncelli.\n", PACKAGE_STRING);

    exit(EXIT_SUCCESS);
}

void usage_and_exit()
{
    printf("%s is a Wake-On-Lan proxy daemon.\n\n"
        "Usage: %s [OPTION]...\n\n"
        "Options:\n"
        "  -h, --help              print this help, then exit.\n"
        "  -v, --version           print version number, then exit.\n"
        "  -f, --foreground        don't fork to background.\n"
        "  -i, --interface=IFACE   destination network interface.\n"
        "  -p, --port=PORT         udp port used for wol packets.\n"
        "\nReport bugs to <%s>.\n",
        PACKAGE_NAME, PACKAGE_NAME, PACKAGE_BUGREPORT);
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
            {0, 0, 0, 0}
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

int main(int argc, char *argv[])
{
    int s, sockbcast, ret;
    char wol_buf[BUFFER_SIZE], mac_buf[6], ip_rmt[16], ip_dst[16];;
    ssize_t rcv_len;
    socklen_t wol_rmt_len;
    struct sockaddr_in wol_rmt, wol_src, wol_dst;
    struct in_addr wol_msk;

    parse_options(argc, argv);

    ret = EXIT_SUCCESS;

    memset(&wol_src, 0, sizeof(struct sockaddr_in));
    memset(&wol_dst, 0, sizeof(struct sockaddr_in));

    wol_src.sin_family = AF_INET;
    wol_src.sin_addr.s_addr = htonl(INADDR_ANY);
    wol_src.sin_port = htons(g_port);

    wol_dst.sin_family = AF_INET;
    if ((ret = ifntoa(g_interface, &wol_dst.sin_addr, SIOCGIFBRDADDR)) < 0) {
        goto exit_fail1;
    }
    wol_dst.sin_port = htons(g_port);

    if ((ret = ifntoa(g_interface, &wol_msk, SIOCGIFNETMASK)) < 0) {
        goto exit_fail1;
    }

    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        ret = s;
        goto exit_fail1;
    }

    sockbcast = 1;

    if ((ret = setsockopt(s, SOL_SOCKET, SO_BROADCAST,
                          &sockbcast, sizeof(sockbcast))) < 0) {
        goto exit_fail2;
    }

    if ((ret = bind(s, (struct sockaddr *) &wol_src, sizeof(wol_src))) < 0) {
        goto exit_fail2;
    }

    memset(ip_rmt, 0, sizeof(ip_rmt));
    memset(ip_dst, 0, sizeof(ip_dst));

    if (g_foreground == 0) background();

    while (1)
    {
        wol_rmt_len = sizeof(wol_rmt);
        memset(&wol_rmt, 0, wol_rmt_len);

        if ((rcv_len = recvfrom(s, wol_buf, sizeof(wol_buf), 0,
            (struct sockaddr *) &wol_rmt, &wol_rmt_len)) < 0) {
            ret = EXIT_FAILURE;
            goto exit_fail2;
        }

        /* avoiding loops and local packets */
        if ((wol_rmt.sin_addr.s_addr & wol_msk.s_addr) ==
                (wol_dst.sin_addr.s_addr & wol_msk.s_addr)) continue;

        if (wol_packet(wol_buf, (size_t) rcv_len, mac_buf) != 0) {
            syslog(LOG_ERR,
                "unknown packed from %s", inet_ntoa(wol_rmt.sin_addr));
            continue;
        }

        strncpy(ip_rmt, inet_ntoa(wol_rmt.sin_addr), sizeof(ip_rmt) - 1);
        strncpy(ip_dst, inet_ntoa(wol_dst.sin_addr), sizeof(ip_dst) - 1);

        syslog(LOG_NOTICE, "forwarding magic packet from %s to %s "
            "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx", ip_rmt, ip_dst,
            mac_buf[0], mac_buf[1], mac_buf[2],
            mac_buf[3], mac_buf[4], mac_buf[5]
        );

        if ((rcv_len = sendto(s, wol_buf, (size_t) rcv_len, 0,
                  (struct sockaddr *) &wol_dst, sizeof(wol_dst))) < 0) {
            ret = EXIT_FAILURE;
            goto exit_fail2;
        }
    }

exit_fail2:
    close(s);

exit_fail1:
    perror(argv[0]);
    return ret;
}

