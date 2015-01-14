/* wolpd - Wake-On-LAN Proxy Daemon
 * Copyright (C) 2015  Charles-Antoine Degennes <cadegenn@gmail.com>
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
#include <dirent.h>                /* readdir(), etc.                    */
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <signal.h>
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


#define DEFAULT_IFACE       "eth0"
#define DEFAULT_PORT        9
#define DEFAULT_PIDFILE     "/var/run/"PACKAGE".pid"

#define ETH_P_WOL           0x0842
#define WOL_MAGIC_LEN       6

uint8_t wol_magic[WOL_MAGIC_LEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

#define MAX_MAC_ADDRESSES   8192
#define MAX_INTERFACES      16        /* 4096 is way too much */

struct eth_frame {
    struct ethhdr       head;
    uint8_t             data[ETH_DATA_LEN];
};

/* create bool type */
typedef int bool;
enum { false, true };

/* global options */
char*       g_iface     = DEFAULT_IFACE;
uint16_t    g_port      = DEFAULT_PORT;
bool        g_foregnd   = false;
char*       g_pidfile   = DEFAULT_PIDFILE;
bool        g_debug     = false;
bool        g_devel     = false;

/*
 * Try to clean som stuff left around
 */
void onExit() {
    syslog(LOG_INFO, "Daemon exiting...");
    /* delete pid file */
    unlink(g_pidfile);
}

/*
 * Register some signals to terminate program
 */
void register_signals() {
    /* register signals */
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = onExit;
    sigaction(SIGHUP, &action, NULL);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    
    /* register the 'on exit' event */
    //atexit(onExit);
}

void version_and_exit()
{
    printf("\
%s\n\n\
Copyright (C) 2010 Federico Simoncelli\n\
License GPLv3+: \
GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.\n\n\
Modified by Charles-Antoine Degennes\n\
Originally written by Federico Simoncelli.\n",
        PACKAGE_STRING);

    exit(EXIT_SUCCESS);
}

void usage_and_exit()
{
    printf("\
%s is a Wake-On-Lan proxy daemon.\n\n\
Usage: %s [OPTION]...\n\n\
Options:\n\
  -d, --debug             print debug informations.\n\
  -D, --devel             print development informations.\n\
  -h, --help              print this help, then exit.\n\
  -v, --version           print version number, then exit.\n\
  -f, --foreground        don't fork to background.\n\
  -i, --interface=IFACE   source network interface (default: %s).\n\
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
            {"debug", 0, 0, 'd'},
            {"devel", 0, 0, 'D'},
            {"help", 0, 0, 'h'},
            {"version", 0, 0, 'v'},
            {"foreground", 0, 0, 'f'},
            {"interface", 1, 0, 'i'},
            {"port", 1, 0, 'p'},
            {NULL, 0, NULL, 0}
        };

        if ((c = getopt_long(argc, argv, "hvi:fp:dD",
                     long_options, &option_index)) == -1) break;

        switch (c) {
            case 'D':
                g_devel = true;
                break;
            case 'd':
                g_debug = true;
                break;
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
                g_foregnd = true;
                break;
            case 'p':
                g_port = (unsigned short)atoi(optarg);
                break;
        }
    }
}

/*
 * Find macaddress in given arrays
 * @param   string      macaddress to search for
 * @param   array       2-dimensional array to look in
 * @param   array       1-dimensional array containing number of element of the 2-dimensional array
 * @return  int         index of the found macaddress, -1 otherwise
 */
int find_macaddress(char *macaddress, char *mac_addresses[MAX_INTERFACES][MAX_MAC_ADDRESSES], int mac_address_cnt[MAX_INTERFACES]) {
    unsigned int i = 0, j = 0;
    for (i=0; i < MAX_INTERFACES; i++) {
        for (j=0; j < mac_address_cnt[i]; j++) {
            if (strcmp(macaddress, mac_addresses[i][j]) == 0) {
                if (g_debug) {
                    syslog(LOG_NOTICE, "Found macaddress %s on interface #%d", macaddress, i);
                    if (g_foregnd) printf("Found macaddress %s on interface #%d\n", macaddress, i);
                }
                return (int)i;
            }
        }
    }
    /* not found */
    return -1;
}
/*
 * Find files based on pattern
 * @param   string      directory to search in
 * @param   string      pattern to match to
 * @param   array       list of files that match pattern
 * @return  int         -1 on failure
 */
int find_configfiles(char *directory, char *pattern, char *filename[]) {
    DIR *dir;                /* pointer to the scanned directory. */
    struct dirent* entry;    /* pointer to one directory entry.   */
    unsigned int i = 0;
    
    /* open the directory for reading */
    dir = opendir(directory);
    if (!dir) {
        //fprintf(stderr, "Cannot read directory '%s': ", cwd);
        syslog(LOG_ERR, "ERROR: Opening directory '%s' failed with error '%s'\n", directory, strerror(errno));
        if (g_foregnd) perror("ERROR: Opening directory failed");
        exit(EXIT_FAILURE);
    }
    
    /* scan the directory, */
    /* matching the pattern for each file name.               */
    while ((entry = readdir(dir))) {
        if (entry->d_name && strstr(entry->d_name, pattern)) {
            filename[i] = entry->d_name;
            //syslog(LOG_DEBUG, "Found %s file", filename[i]);
            i++;
            //printf("%s/%s\n", cwd, entry->d_name);
        }
    }
    /* add the last entry "NULL". We will use it further to check the end of array */
    filename[i] = NULL;
    
    if (i == 0) { // no match were found so, no file found
        return -1;
    } else {
        return 0;
    }
}

/*
 * get the extension of a filename
 * @param   string      filename
 * @return  string      extension
 */
char *get_filename_ext(char *filename) {
    char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

/*
 * stringify a binary string to hex string
 * @param   string  binary string
 * @return  string  hexadecimal string
 */
char *binToHex(char *bin, size_t length) {
    //size_t length = strlen(bin);
    char *hex = malloc(sizeof (char*) * length);
    strcpy(hex, "\0");
    char *chr = malloc(sizeof (char*));
    size_t i = 0;
    for (i = 0; i < length; i++) {
        if (bin[i] == '\0') break;
        sprintf(chr, "%2.2hhx", bin[i]);
        strcat(hex, chr);
        //printf("'%2.2hhx' => '%s' => '%s' \n", bin[i], chr, hex);
    }
    
    /*if (g_devel) {
        syslog(LOG_DEBUG, "binToHex(): %s => %s", bin, hex);
        if (g_foregnd) printf("binToHex(): %s => %s\n", bin, hex);
    }*/
    
    free(chr);
    free(hex);
    
    return hex;
}

/*
 * Read config file and store data in arrays
 * @param    string     config filename to read
 * @param    array      array of mac addresses read
 * @param    int        count of mac addresses read
 */
void read_config_per_interface(char *config_filename, char *mac_addresses[], int *mac_address_cnt) {
    if (g_debug) {
        syslog(LOG_DEBUG, "Try to read %s", config_filename);
        if (g_foregnd) printf("Try to read %s\n", config_filename);
    }
    FILE *fp;
    //char mac_address_str [17];
    char *mac_address_str = malloc(sizeof (char*) * 18);
    //int i;

    /* Open and read in the MAC addresses from the configuration file */
    if ((fp = fopen (config_filename, "r")) == NULL)
    {
        sleep (1);
        syslog (LOG_INFO, "Failed to open configuration file %s\n", config_filename);
        exit (EXIT_FAILURE);
    }
    else
    {
        /* Read up to MAX_MAC_ADDRESSES from the config file into the MAC addreess array*/
        *mac_address_cnt = 0;
        while ((fgets (mac_address_str, 18, fp) != NULL) && (*mac_address_cnt < MAX_MAC_ADDRESSES))
        {
            int a[6];
            //unsigned int a[6];
            //if (sscanf(mac_address_str, "%02X:%02X:%02X:%02X:%02X:%02X", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5]) == 6)
            if (sscanf(mac_address_str, "%02x:%02x:%02x:%02x:%02x:%02x", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5]) == 6)
            {
                char *mac_address_compressed = malloc(sizeof (char*) * 12);        /* mac address without : char is 12 char long) */
                mac_addresses[*mac_address_cnt] = malloc(sizeof (char*) * 12);
                sprintf(mac_address_compressed, "%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx", a[0], a[1], a[2], a[3], a[4], a[5]);
                sprintf(mac_addresses[*mac_address_cnt], "%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx", a[0], a[1], a[2], a[3], a[4], a[5]);
                /*for (i=0; i<6;i++)
                {
                    //mac_addresses [*mac_address_cnt][i] = a[i];
                    //memcpy(mac_addresses[*mac_address_cnt][i], (char*)&a[i], sizeof(int));
                    mac_addresses [*mac_address_cnt][i] = (char*)a[i];
                }*/
                //syslog (LOG_INFO, "Found mac addresse %s\n", mac_addresses[*mac_address_cnt]);
                if (g_debug) printf("found %s stored as %s\n", mac_address_str, mac_address_compressed);
                (*mac_address_cnt)++;
                
                free(mac_address_compressed);
                free(mac_addresses[*mac_address_cnt]);
            }
            else
            {
                syslog(LOG_INFO, "Error in configuration file at line %d : '%s'", *mac_address_cnt, mac_address_str);
                printf("Error in configuration file at line %d : '%s'\n", *mac_address_cnt, mac_address_str);
            }
            // read until end of line
            while (fgetc(fp) != '\n') {};
        }
    }
    fclose (fp);
    syslog(LOG_INFO, "Found %d mac addresses in %s", *mac_address_cnt, config_filename);
    printf("Found %d mac addresses in %s\n", *mac_address_cnt, config_filename);
    free(mac_address_str);
}

/*
 * Initialize outgoing interfaces
 * @param   string      name of the interface as return by ifconfig
 * @param   struct      sockaddr_ll
 * @return  int         socket
 */
struct sockaddr_ll init_wol_dst(char *name) {
    if (g_debug) {
        syslog(LOG_DEBUG, "Try to connect to %s interface...", name);
        if (g_foregnd) printf("Try to connect to %s interface...\n", name);
    }
    int iface_socket;
    struct ifreq ifhw;
    struct sockaddr_ll layer2;

    /* initializing interface by name */
    strncpy(ifhw.ifr_name, name, sizeof(ifhw.ifr_name));
    memset(&layer2, 0, sizeof(layer2));

    /* create a socket */
    if ((iface_socket = socket(PF_PACKET, SOCK_RAW, 0)) < 0 ) {
        syslog(LOG_ERR, "ERROR: socket() %s", strerror(errno));
        if (g_foregnd) perror("ERROR: socket()");
        //layer2.sll_ifindex = -1;
        return layer2;
    }
    /* request mac address of interface to be sure it is really present */
    if (ioctl(iface_socket, SIOCGIFHWADDR, &ifhw) < 0) {
        syslog(LOG_ERR, "ERROR: ioctl() %s: %s", name, strerror(errno));
        if (g_foregnd) perror("ERROR: ioctl()");
        //layer2.sll_ifindex = -1;
        return layer2;
    }
    /* request index of interface */
    if (ioctl(iface_socket, SIOCGIFINDEX, &ifhw) < 0) {
        syslog(LOG_ERR, "ERROR: ioctl() %s: %s", name, strerror(errno));
        if (g_foregnd) perror("ERROR: ioctl()");
        return layer2;
    }
    /* close socket */
    if (close(iface_socket) < 0) {
        syslog(LOG_ERR, "ERROR: close() %s", strerror(errno));
        if (g_foregnd) perror("ERROR: close()");
        return layer2;
    }
    
    layer2.sll_family  = AF_PACKET;
    layer2.sll_ifindex = ifhw.ifr_ifindex;
    layer2.sll_halen   = ETH_ALEN;

    /*
     * DEVEL
     */
    if (g_devel) {
        printf("DBG: layer2.sll_family  = %d\n", layer2.sll_family);
        printf("DBG: layer2.sll_ifindex = %d\n", layer2.sll_ifindex);
        printf("DBG: layer2.sll_halen   = %d\n", layer2.sll_halen);
    }
    
    return layer2;
}

/*
 * Initialize incoming interface and bind to it
 * @return    int        incoming socket
 */
int init_wol_src() {
    if (g_debug) {
        syslog(LOG_DEBUG, "Try to bind to %s interface...", g_iface);
        if (g_foregnd) printf("Try to bind to %s interface...\n", g_iface);
    }
    struct ifreq ifhw;
    struct sockaddr_in wol_src;
    int in_socket;
    const int optVal = 1;
    char *ip_address = malloc(sizeof (char*) * INET_ADDRSTRLEN);

    /*  create the socket */
    //if ((in_socket = socket(PF_PACKET, SOCK_RAW, 0)) < 0 ) {
    if ((in_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        syslog(LOG_ERR, "ERROR: socket() %s", strerror(errno));
        perror("ERROR: socket() ");
        exit(EXIT_FAILURE);
    }
    //setsockopt(in_socket, SOL_SOCKET, SO_REUSEADDR, (void*) &optVal, optLen);
    setsockopt(in_socket, SOL_SOCKET, SO_REUSEADDR, (void*) &optVal, sizeof(optVal));
    
    /* initialize interface by name */
    memset(&ifhw, 0, sizeof(struct ifreq));
    ifhw.ifr_addr.sa_family = AF_INET;
    strncpy(ifhw.ifr_name, g_iface, sizeof(ifhw.ifr_name));
    if (ioctl(in_socket, SIOCGIFADDR, &ifhw) == -1) {
        syslog(LOG_ERR, "ERROR: ioctl() %s: %s", g_iface, strerror(errno));
        perror("ERROR: ioctl() ");
        exit(EXIT_FAILURE);
    }
    
    /* get ipaddress */
    inet_ntop(AF_INET, &ifhw.ifr_addr.sa_data[2], ip_address, INET_ADDRSTRLEN);
    syslog(LOG_INFO, "Found address %s at %s", ip_address, g_iface);
    if (g_foregnd) printf("Found address %s at %s\n", ip_address, g_iface);
    memset(&wol_src, 0, sizeof(wol_src));
    wol_src.sin_family      = AF_INET;
    //wol_src.sin_addr.s_addr = htonl(INADDR_ANY);
    wol_src.sin_addr.s_addr = inet_addr(ip_address);
    wol_src.sin_port        = htons(g_port);
    
    /* bind socket to interface */
    if (bind(in_socket, (struct sockaddr *) &wol_src, sizeof(wol_src)) < 0) {
        syslog(LOG_ERR, "ERROR: bind() %d: %s", errno, strerror(errno));
        perror("ERROR: couldn't bind to local interface");
        if (close(in_socket) < 0) {
            syslog(LOG_ERR, "ERROR: close() %d: %s", errno, strerror(errno));
            perror("ERROR: couldn't close socket");
        }
        exit(EXIT_FAILURE);
    }
    syslog(LOG_INFO, "Listening on %s %s:%d", g_iface, ip_address, g_port);
    if (g_foregnd) printf("Listening on %s %s:%d\n", g_iface, ip_address, g_port);
    
    free(ip_address);
    
    return in_socket;
}

/*
 * initialize wol packet
 * @return   eth_frame   wol message
 */
struct eth_frame init_wol_msg() {
    struct eth_frame wol_msg;
    struct ifreq ifhw;
    int in_socket;

    /*  create the socket */
    //if ((in_socket = socket(PF_PACKET, SOCK_RAW, 0)) < 0 ) {
    if ((in_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        syslog(LOG_ERR, "ERROR: socket() %s", strerror(errno));
        perror("ERROR: socket() ");
        exit(EXIT_FAILURE);
    }

    /* initialize interface by name */
    memset(&ifhw, 0, sizeof(struct ifreq));
    ifhw.ifr_addr.sa_family = AF_INET;
    strncpy(ifhw.ifr_name, g_iface, sizeof(ifhw.ifr_name));

    /* request hw address */
    if (ioctl(in_socket, SIOCGIFHWADDR, &ifhw) == -1) {
        syslog(LOG_ERR, "ERROR: ioctl() %s: %s", g_iface, strerror(errno));
        perror("ERROR: ioctl() ");
        exit(EXIT_FAILURE);
    }

    memcpy(wol_msg.head.h_source, ifhw.ifr_hwaddr.sa_data, ETH_ALEN);
    
    /*
     * DEVEL
     */
    if (g_devel) {
        printf("DBG: ifhw.ifr_hwaddr.sa_data = %s\n", binToHex((char*)ifhw.ifr_hwaddr.sa_data, ETH_ALEN));
    }
    
    /* set protocol */
    wol_msg.head.h_proto = htons(ETH_P_WOL);

    return wol_msg;
}

int main(int argc, char *argv[])
{
    int out_socket, in_socket;
    struct eth_frame wol_msg;
    ssize_t wol_len;
    struct sockaddr_in wol_rmt;
    //struct sockaddr_ll wol_dst;
    struct sockaddr_ll wol_dst_int[MAX_INTERFACES];
    socklen_t wol_rmt_len;
    char *config_full_path_name = malloc(sizeof (char*) * 256);            /* temporary variable to store full path name of current config filename */
    char *config_filenames[MAX_INTERFACES];    /* array of config filenames : 1 per interface */
    char *mac_addresses[MAX_INTERFACES][MAX_MAC_ADDRESSES];
    int mac_address_cnt[MAX_INTERFACES];
    char *interface_names[MAX_INTERFACES];
    char *mac_address = malloc(sizeof (char*) * 18);
    int i = 0;

    parse_options(argc, argv);
    
    /* register on exit and signals function */
    register_signals();
    
    /* search for list of mac address per vlan in configuration files 
     * config file must be named /etc/wolpd.${interface_name}
     */
    if (find_configfiles("/etc", PACKAGE, config_filenames) < 0) {
        perror("ERROR: No config filenames found in /etc");
        exit(EXIT_FAILURE);
    }

    /* try to connect to interface to see if it exist
     * and if so, load list of mac addresses from config files */
    i = 0;
    while (config_filenames[i] != NULL) {
        interface_names[i] = get_filename_ext(config_filenames[i]);
        sprintf(config_full_path_name, "/etc/%s", config_filenames[i]);
        wol_dst_int[i] = init_wol_dst(interface_names[i]);
        if (wol_dst_int[i].sll_ifindex >= 0) {
            read_config_per_interface(config_full_path_name, mac_addresses[i], &mac_address_cnt[i]);
        } else {
            syslog(LOG_INFO, "Interface %s does not exist. No need to read %s", interface_names[i], config_full_path_name);
            if (g_foregnd) printf("Interface %s does not exist. No need to read %s\n", interface_names[i], config_full_path_name);
        }
        i++;
        if (i >= MAX_INTERFACES) {
            syslog(LOG_ERR, "ERROR: you reached maximum interfaces number allowed (%d). Try to increase MAX_INTERFACES in source code, recompile and retry.", MAX_INTERFACES);
            if (g_foregnd) perror("ERROR: you reached maximum interfaces number allowed. Try to increase MAX_INTERFACES in source code, recompile and retry.");
            syslog(LOG_ERR, "ERROR: %s will continue with your first %d interfaces found", PACKAGE, MAX_INTERFACES);
            break;
        }
    }

    /* this socket will be use for outgoing packets */
    if ((out_socket = socket(PF_PACKET, SOCK_RAW, 0)) < 0 ) {
        perror("ERROR: couldn't open external socket");
        exit(EXIT_FAILURE);
    }

    in_socket = init_wol_src();
    wol_msg = init_wol_msg();
    
    if (g_foregnd == 0) {
        if (g_debug) syslog(LOG_DEBUG, "DBG: daemonize()");
        if (daemon(0, 0) < 0) {
            syslog(LOG_ERR, "ERROR: daemon() %d: %s", errno, strerror(errno));
            perror("ERROR: cannot daemonize");
        };
    }
    
    /* daemon or not, display and write pid to file */
    if (g_debug) {
        syslog(LOG_DEBUG, "DBG: get pid %d", getpid());
        printf("DBG: get pid %d\n", getpid());
    }
    FILE *pid_file = fopen(g_pidfile, "w+");
    if (pid_file < 0) {
        syslog(LOG_ERR, "ERROR: unable to open() %s: %d: %s", g_pidfile, errno, strerror(errno));
        if (g_foregnd) perror("ERROR: unable to open() pidfile");
    } else {
        if (fprintf(pid_file, "%d\n", getpid()) < 0) {
            syslog(LOG_WARNING, "WARN: unable to write to %s: %d: %s", g_pidfile, errno, strerror(errno));
            if (g_foregnd) perror("WARN: unable to write() to pidfile");
        }
        if (fclose(pid_file) < 0) {
            syslog(LOG_WARNING, "WARN: unable to close() %s: %d: %s", g_pidfile, errno, strerror(errno));
            if (g_foregnd) perror("WARN: unable to close() to pidfile");
        }
        
    }
    
    free(config_full_path_name);
    
    syslog(LOG_DEBUG, "Waiting for incoming magic packets...");
    if (g_foregnd) printf("Waiting for incoming magic packets...\n");
    while (1)
    {
        i = 0;
        wol_rmt_len = sizeof(wol_rmt);

        if ((wol_len = recvfrom(in_socket, wol_msg.data, ETH_DATA_LEN, 0, (struct sockaddr *) &wol_rmt, &wol_rmt_len)) < 0) {
            syslog(LOG_ERR,"ERROR: recvfrom() %d: %s", errno, strerror(errno));
            //if (g_foregnd) perror("ERROR: couldn't receive data from incoming socket");
            if (g_foregnd) perror("ERROR: recvfrom()");
            exit(EXIT_FAILURE);
        }
        /*
         * DEVEL
         */
        if (g_devel) {
            printf("DBG: wol_rmt.sin_addr = %s\n", inet_ntoa(wol_rmt.sin_addr));
            printf("DBG: wol_rmt.sin_port = %d\n", ntohs(wol_rmt.sin_port));
        }

        if (wol_len < WOL_MAGIC_LEN + ETH_ALEN) {
            syslog(LOG_ERR, "packet too short from %s", inet_ntoa(wol_rmt.sin_addr));
            if (g_foregnd) fprintf(stderr, "ERROR: packet too short from %s\n", inet_ntoa(wol_rmt.sin_addr));
            continue;
        }

        if (memcmp(wol_msg.data, wol_magic, WOL_MAGIC_LEN) != 0) {
            syslog(LOG_ERR, "ERROR: unknown packed from %s", inet_ntoa(wol_rmt.sin_addr));
            if (g_foregnd) fprintf(stderr, "ERROR: unknown packed from %s\n", inet_ntoa(wol_rmt.sin_addr));
            continue;
        }

        memcpy(wol_msg.head.h_dest,     wol_msg.data + WOL_MAGIC_LEN, ETH_ALEN);

        sprintf(mac_address, "%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx", 
            wol_msg.head.h_dest[0], wol_msg.head.h_dest[1],
            wol_msg.head.h_dest[2], wol_msg.head.h_dest[3],
            wol_msg.head.h_dest[4], wol_msg.head.h_dest[5]);
        /* look for destination mac address in our config files */
        if ((i = find_macaddress(mac_address, mac_addresses, mac_address_cnt)) < 0) {
            syslog(LOG_ERR, "destination address %s unknown from config files... ignoring", mac_address);
            if (g_foregnd) fprintf(stderr, "destination address %s unknown from config files... ignoring\n", mac_address);
            /* no need to go further */
            continue;
        }
        if (g_debug) {
            syslog(LOG_INFO,"Found macaddress %s on interface %s", mac_address, interface_names[i]);
            if (g_foregnd) printf("Found macaddress %s on interface %s\n", mac_address, interface_names[i]);
        }
        memcpy(wol_dst_int[i].sll_addr, wol_msg.data + WOL_MAGIC_LEN, ETH_ALEN);

        /*
         * DEBUG
         */
        if (g_devel) {
            printf("DBG: wol_msg.head.h_dest    = %s\n", binToHex((char*)wol_msg.head.h_dest, ETH_ALEN));
            printf("DBG: wol_msg.head.h_source  = %s\n", binToHex((char*)wol_msg.head.h_source, ETH_ALEN));
            printf("DBG: wol_msg.head.h_proto   = %#2.4x\n", ntohs(wol_msg.head.h_proto));
            printf("DBG: wol_msg.data           = %s\n", binToHex((char*)wol_msg.data, ETH_DATA_LEN));
            printf("DBG: wol_dst_int[%d].sll_addr    = %s\n", i, binToHex((char*)wol_dst_int[i].sll_addr, ETH_ALEN));
            printf("DBG: wol_dst_int[%d].sll_family  = %d\n", i, wol_dst_int[i].sll_family);
            printf("DBG: wol_dst_int[%d].sll_ifindex = %d\n", i, wol_dst_int[i].sll_ifindex);
            printf("DBG: wol_dst_int[%d].sll_halen   = %d\n", i, wol_dst_int[i].sll_halen);
        }
        
        if ((wol_len = sendto(
                out_socket, &wol_msg, (size_t) wol_len + ETH_HLEN, 0,
                    (struct sockaddr *) &wol_dst_int[i], sizeof(wol_dst_int[i]))) < 0) {
            syslog(LOG_ERR,"ERROR: sendto() %d: %s", errno, strerror(errno));
            if (g_foregnd) perror("ERROR: sendto(): couldn't forward data to outgoing socket");
            exit(EXIT_FAILURE);
        }

        syslog(LOG_NOTICE, "magic packet from %s forwarded to "
            "%2.2hhx:%2.2hhx:%2.2hhx:%2.2hhx:%2.2hhx:%2.2hhx via interface %s",
            inet_ntoa(wol_rmt.sin_addr),
            wol_msg.head.h_dest[0], wol_msg.head.h_dest[1],
            wol_msg.head.h_dest[2], wol_msg.head.h_dest[3],
            wol_msg.head.h_dest[4], wol_msg.head.h_dest[5],
            interface_names[i]
        );
        if (g_foregnd) printf("magic packet from %s forwarded to "
            "%2.2hhx:%2.2hhx:%2.2hhx:%2.2hhx:%2.2hhx:%2.2hhx via interface %s\n",
            inet_ntoa(wol_rmt.sin_addr),
            wol_msg.head.h_dest[0], wol_msg.head.h_dest[1],
            wol_msg.head.h_dest[2], wol_msg.head.h_dest[3],
            wol_msg.head.h_dest[4], wol_msg.head.h_dest[5],
            interface_names[i]
        );
    }

}

