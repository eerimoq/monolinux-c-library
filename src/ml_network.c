/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019, Erik Moqvist
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * This file is part of the Monolinux C library project.
 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <net/route.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include "ml/ml.h"

#define VERDICT_ACCEPT  (-NF_ACCEPT - 1)
#define VERDICT_DROP    (-NF_DROP - 1)
#define VERDICT_QUEUE   (-NF_QUEUE - 1)

struct standard_entry_t {
    struct ipt_entry entry;
    struct xt_standard_target standard;
};

struct error_entry_t {
    struct ipt_entry entry;
    struct xt_error_target error;
};

struct replace_t {
    struct ipt_replace header;
    struct standard_entry_t standard[3];
    struct error_entry_t error;
};

static int net_open(const char *name_p,
                    struct ifreq *ifreq_p)
{
    int netfd;

    netfd = ml_socket(AF_INET, SOCK_DGRAM, 0);

    memset(ifreq_p, 0, sizeof(*ifreq_p));
    strncpy(&ifreq_p->ifr_name[0], name_p, sizeof(ifreq_p->ifr_name) - 1);
    ifreq_p->ifr_name[sizeof(ifreq_p->ifr_name) - 1] = '\0';

    return (netfd);
}

static void net_close(int netfd)
{
    close(netfd);
}

static void create_address_request(struct ifreq *ifreq_p,
                                   const char *address_p)
{
    struct sockaddr_in sai;

    memset(&sai, 0, sizeof(sai));
    sai.sin_family = AF_INET;
    sai.sin_port = 0;
    sai.sin_addr.s_addr = inet_addr(address_p);
    memcpy(&ifreq_p->ifr_addr, &sai, sizeof(ifreq_p->ifr_addr));
}

static int up(int netfd, struct ifreq *ifreq_p)
{
    int res;

    res = ml_ioctl(netfd, SIOCGIFFLAGS, ifreq_p);

    if (res == 0) {
        ifreq_p->ifr_flags |= IFF_UP;
        res = ml_ioctl(netfd, SIOCSIFFLAGS, ifreq_p);
    }

    if (res != 0) {
        res = -errno;
    }

    return (res);
}

static int down(int netfd, struct ifreq *ifreq_p)
{
    int res;

    res = ml_ioctl(netfd, SIOCGIFFLAGS, ifreq_p);

    if (res == 0) {
        ifreq_p->ifr_flags &= ~IFF_UP;
        res = ml_ioctl(netfd, SIOCSIFFLAGS, ifreq_p);
    }

    if (res != 0) {
        res = -errno;
    }

    return (res);
}

static int set_ip_address(int netfd,
                          struct ifreq *ifreq_p,
                          const char *address_p)
{
    create_address_request(ifreq_p, address_p);

    return (ml_ioctl(netfd, SIOCSIFADDR, ifreq_p));
}

static int set_netmask(int netfd,
                       struct ifreq *ifreq_p,
                       const char *netmask_p)
{
    create_address_request(ifreq_p, netmask_p);

    return (ml_ioctl(netfd, SIOCSIFNETMASK, ifreq_p));
}

static int set_mtu(int netfd,
                   struct ifreq *ifreq_p,
                   int mtu)
{
    ifreq_p->ifr_ifru.ifru_mtu = mtu;

    return (ml_ioctl(netfd, SIOCSIFMTU, ifreq_p));
}

static int get_ifreq(const char *name_p,
                     int request,
                     struct ifreq *ifreq_p)
{
    int res;
    int netfd;

    res = -EGENERAL;
    netfd = net_open(name_p, ifreq_p);

    if (netfd != -1) {
        res = ioctl(netfd, request, ifreq_p);
        net_close(netfd);
    }

    return (res);
}

static const char *ipt_set_option_as_string(int optname)
{
    const char *res_p;

    switch (optname) {

    case IPT_SO_SET_REPLACE:
        res_p = "IPT_SO_SET_REPLACE";
        break;

    default:
        res_p = "*** unknown ***";
        break;
    }

    return (res_p);
}

static const char *ipt_get_option_as_string(int optname)
{
    const char *res_p;

    switch (optname) {

    case IPT_SO_GET_INFO:
        res_p = "IPT_SO_GET_INFO";
        break;

    case IPT_SO_GET_ENTRIES:
        res_p = "IPT_SO_GET_ENTRIES";
        break;

    default:
        res_p = "*** unknown ***";
        break;
    }

    return (res_p);
}

static int set_filter(int domain, int optname, const void *buf_p, size_t size)
{
    int sockfd;
    int res;

    sockfd = ml_socket(domain, SOCK_RAW, IPPROTO_RAW);

    if (sockfd != -1) {
        res = setsockopt(sockfd, SOL_IP, optname, buf_p, size);

        if (res == -1) {
            res = -errno;
        }

        close(sockfd);
    } else {
        res = -errno;
    }

    if (res != 0) {
        ml_info("network: Set filter option %s failed with: %s",
                ipt_set_option_as_string(optname),
                strerror(errno));
    }

    return (res);
}

static int get_filter(int domain, int optname, void *buf_p, socklen_t *size_p)
{
    int sockfd;
    int res;

    res = -EGENERAL;
    sockfd = ml_socket(domain, SOCK_RAW, IPPROTO_RAW);

    if (sockfd != -1) {
        res = getsockopt(sockfd, SOL_IP, optname, buf_p, size_p);
        close(sockfd);
    }

    if (res != 0) {
        ml_info("network: Get filter option %s failed with: %s",
                ipt_get_option_as_string(optname),
                strerror(errno));
    }

    return (res);
}

static int get_info(const char *table_p, struct ipt_getinfo *info_p)
{
    socklen_t size;

    strcpy(&info_p->name[0], table_p);
    size = sizeof(*info_p);

    return (get_filter(AF_INET, IPT_SO_GET_INFO, info_p, &size));
}

static int get_info_ipv6(const char *table_p, struct ip6t_getinfo *info_p)
{
    socklen_t size;

    strcpy(&info_p->name[0], table_p);
    size = sizeof(*info_p);

    return (get_filter(AF_INET6, IP6T_SO_GET_INFO, info_p, &size));
}

static void init_replace(struct replace_t *replace_p,
                         struct ipt_getinfo *info_p)
{
    struct ipt_replace *header_p;

    memset(replace_p, 0, sizeof(*replace_p));
    header_p = &replace_p->header;
    strcpy(&header_p->name[0], "filter");
    header_p->valid_hooks = ((1 << NF_IP_LOCAL_IN)
                             | (1 << NF_IP_FORWARD)
                             | (1 << NF_IP_LOCAL_OUT));
    header_p->num_entries = 4;
    header_p->size = sizeof(*replace_p) - sizeof(replace_p->header);
    header_p->hook_entry[NF_INET_LOCAL_IN] = 0;
    header_p->hook_entry[NF_INET_FORWARD] = sizeof(replace_p->standard[0]);
    header_p->hook_entry[NF_INET_LOCAL_OUT] = 2 * sizeof(replace_p->standard[0]);
    header_p->underflow[NF_INET_LOCAL_IN] = 0;
    header_p->underflow[NF_INET_FORWARD] = sizeof(replace_p->standard[0]);
    header_p->underflow[NF_INET_LOCAL_OUT] = 2 * sizeof(replace_p->standard[0]);
    header_p->num_counters = info_p->num_entries;
}

static void fill_standard_entry(struct standard_entry_t *entry_p, int verdict)
{
    entry_p->entry.target_offset = offsetof(struct standard_entry_t, standard);
    entry_p->entry.next_offset = sizeof(*entry_p);
    entry_p->standard.target.u.target_size = sizeof(entry_p->standard);
    entry_p->standard.verdict = verdict;
}

static void fill_error_entry(struct error_entry_t *entry_p)
{
    entry_p->entry.target_offset = offsetof(struct error_entry_t, error);
    entry_p->entry.next_offset = sizeof(*entry_p);
    entry_p->error.target.u.user.target_size = sizeof(entry_p->error);
    strcpy(&entry_p->error.target.u.user.name[0], "ERROR");
    strcpy(&entry_p->error.errorname[0], "ERROR");
}

static int filter_apply_all(int verdict)
{
    int res;
    struct ipt_getinfo info;
    struct replace_t replace;

    res = get_info("filter", &info);

    if (res != 0) {
        return (res);
    }

    init_replace(&replace, &info);
    fill_standard_entry(&replace.standard[0], verdict);
    fill_standard_entry(&replace.standard[1], verdict);
    fill_standard_entry(&replace.standard[2], verdict);
    fill_error_entry(&replace.error);

    return (ml_network_filter_ipv4_set(&replace.header));
}

static const char *format_ipv4(char *buf_p, uint32_t address)
{
    sprintf(buf_p,
            "%u.%u.%u.%u",
            (address >> 24) & 0xff,
            (address >> 16) & 0xff,
            (address >> 8) & 0xff,
            (address >> 0) & 0xff);

    return (buf_p);
}

static int command_ifconfig_print(const char *name_p)
{
    int res;
    uint8_t mac_address[6];
    int index;
    struct in_addr in_addr;

    printf("IPAddress:  ");
    res = ml_network_interface_ip_address(name_p, &in_addr);

    if (res == 0) {
        puts(inet_ntoa(in_addr));
    } else {
        printf("failure\n");
    }

    printf("MACAddress: ");
    res = ml_network_interface_mac_address(name_p, &mac_address[0]);

    if (res == 0) {
        printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
               mac_address[0],
               mac_address[1],
               mac_address[2],
               mac_address[3],
               mac_address[4],
               mac_address[5]);
    } else {
        printf("failure\n");
    }

    printf("MTU:        ");
    res = ml_network_interface_mtu(name_p);

    if (res > 0) {
        printf("%d\n", res);
    } else {
        printf("failure\n");
    }

    printf("Index:      ");
    res = ml_network_interface_index(name_p, &index);

    if (res == 0) {
        printf("%d\n", index);
    } else {
        printf("failure\n");
    }

    return (0);
}

static int command_ifconfig(int argc, const char *argv[])
{
    int res;

    res = -EINVAL;

    if (argc == 2) {
        res = command_ifconfig_print(argv[1]);
    } else if (argc == 3) {
        if (strcmp(argv[2], "up") == 0) {
            res = ml_network_interface_up(argv[1]);
        } else if (strcmp(argv[2], "down") == 0) {
            res = ml_network_interface_down(argv[1]);
        }
    } else if (argc == 5) {
        res = ml_network_interface_configure(argv[1],
                                             argv[2],
                                             argv[3],
                                             atoi(argv[4]));
    }

    if (res != 0) {
        printf("Usage: ifconfig <interface>\n"
               "       ifconfig <interface> up/down\n"
               "       ifconfig <interface> <ip-address> <netmask> <mtu>\n");
    }

    return (res);
}

static int command_route(int argc, const char *argv[])
{
    int res;

    res = -EINVAL;

    if (argc == 3) {
        res = ml_network_interface_add_route(argv[1], argv[2]);
    }

    if (res != 0) {
        printf("Usage: route <interface> <ip-address>\n");
    }

    return (res);
}

static const char *duplex_string(int duplex)
{
    const char *res_p;

    switch (duplex) {

    case DUPLEX_HALF:
        res_p = "half";
        break;

    case DUPLEX_FULL:
        res_p = "full";
        break;

    case DUPLEX_UNKNOWN:
        res_p = "unknown";
        break;

    default:
        res_p = "invalid";
        break;
    }

    return (res_p);
}

static const char *autoneg_string(int autoneg)
{
    const char *res_p;

    switch (autoneg) {

    case AUTONEG_DISABLE:
        res_p = "off";
        break;

    case AUTONEG_ENABLE:
        res_p = "on";
        break;

    default:
        res_p = "invalid";
        break;
    }

    return (res_p);
}

static int command_ethtool_print(const char *name_p)
{
    struct ethtool_cmd settings;
    struct ifreq ifreq;
    int res;
    int netfd;

    netfd = net_open(name_p, &ifreq);

    if (netfd == -1) {
        return (-errno);
    }

    settings.cmd = ETHTOOL_GSET;
    ifreq.ifr_data = (char *)&settings;

    res = ioctl(netfd, SIOCETHTOOL, &ifreq);

    if (res == -1) {
        res = -errno;

        goto out;
    }

    printf("Speed:           %u Mbps\n", (unsigned)ethtool_cmd_speed(&settings));
    printf("Duplex:          %s\n", duplex_string(settings.duplex));
    printf("Autonegotiation: %s\n", autoneg_string(settings.autoneg));

    net_close(netfd);

    return (0);

 out:
    net_close(netfd);

    return (res);
}

static bool is_changed(const char *arg_p)
{
    return (strcmp(arg_p, "-") != 0);
}

static int command_ethtool_link_configure(int argc, const char *argv[])
{
    int speed;
    int duplex;
    int autoneg;

    if (argc != 5) {
        return (-EINVAL);
    }

    if (is_changed(argv[2])) {
        speed = atoi(argv[2]);
    } else {
        speed = -1;
    }

    if (is_changed(argv[3])) {
        if (strcmp(argv[3], "half") == 0) {
            duplex = DUPLEX_HALF;
        } else if (strcmp(argv[3], "full") == 0) {
            duplex = DUPLEX_FULL;
        } else {
            return (-EINVAL);
        }
    } else {
        duplex = -1;
    }

    if (is_changed(argv[4])) {
        if (strcmp(argv[4], "on") == 0) {
            autoneg = AUTONEG_ENABLE;
        } else if (strcmp(argv[4], "off") == 0) {
            autoneg = AUTONEG_DISABLE;
        } else {
            return (-EINVAL);
        }
    } else {
        autoneg = -1;
    }

    return (ml_network_interface_link_configure(argv[1],
                                                speed,
                                                duplex,
                                                autoneg));
}

static int command_ethtool(int argc, const char *argv[])
{
    int res;

    if (argc == 2) {
        res = command_ethtool_print(argv[1]);
    } else {
        res = command_ethtool_link_configure(argc, argv);
    }

    if (res != 0) {
        printf("Usage: ethtool <interface>\n");
        printf("       ethtool <interface> <speed> <duplex> <autoneg>\n");
        printf("         where\n");
        printf("           <speed> is the speed in Mbps or -\n");
        printf("           <duplex> is half, full or -\n");
        printf("           <autoneg> is on, off or -\n");
    }

    return (res);
}

void ml_network_init(void)
{
    ml_shell_register_command("ifconfig",
                              "Network interface management.",
                              command_ifconfig);
    ml_shell_register_command("route",
                              "Network routing.",
                              command_route);
    ml_shell_register_command("ethtool",
                              "Ethernet link settings.",
                              command_ethtool);
}

int ml_network_interface_configure(const char *name_p,
                                   const char *ipv4_address_p,
                                   const char *ipv4_netmask_p,
                                   int mtu)
{
    struct ifreq ifreq;
    int res;
    int netfd;

    netfd = net_open(name_p, &ifreq);

    if (netfd == -1) {
        return (-EGENERAL);
    }

    res = set_ip_address(netfd, &ifreq, ipv4_address_p);

    if (res != 0) {
        goto out;
    }

    res = set_netmask(netfd, &ifreq, ipv4_netmask_p);

    if (res != 0) {
        goto out;
    }

    res = set_mtu(netfd, &ifreq, mtu);

 out:
    net_close(netfd);

    return (res);
}

int ml_network_interface_up(const char *name_p)
{
    struct ifreq ifreq;
    int res;
    int netfd;

    res = -EGENERAL;
    netfd = net_open(name_p, &ifreq);

    if (netfd != -1) {
        res = up(netfd, &ifreq);
        net_close(netfd);
    }

    return (res);
}

int ml_network_interface_down(const char *name_p)
{
    struct ifreq ifreq;
    int res;
    int netfd;

    res = -EGENERAL;
    netfd = net_open(name_p, &ifreq);

    if (netfd != -1) {
        res = down(netfd, &ifreq);
        net_close(netfd);
    }

    return (res);
}

int ml_network_interface_index(const char *name_p, int *index_p)
{
    struct ifreq ifreq;
    int res;

    res = get_ifreq(name_p, SIOCGIFINDEX, &ifreq);

    if (res == 0) {
        *index_p = ifreq.ifr_ifindex;
    }

    return (res);
}

int ml_network_interface_mac_address(const char *name_p,
                                     uint8_t *mac_address_p)
{
    struct ifreq ifreq;
    int res;

    res = get_ifreq(name_p, SIOCGIFHWADDR, &ifreq);

    if (res == 0) {
        memcpy(mac_address_p, &ifreq.ifr_hwaddr.sa_data[0], 6);
    }

    return (res);
}

int ml_network_interface_ip_address(const char *name_p,
                                    struct in_addr *ip_address_p)
{
    struct ifreq ifreq;
    int res;

    res = get_ifreq(name_p, SIOCGIFADDR, &ifreq);

    if (res == 0) {
        *ip_address_p = ((struct sockaddr_in *)&ifreq.ifr_addr)->sin_addr;
    }

    return (res);
}

int ml_network_interface_mtu(const char *name_p)
{
    struct ifreq ifreq;
    int res;

    res = get_ifreq(name_p, SIOCGIFMTU, &ifreq);

    if (res == 0) {
        res = ifreq.ifr_ifru.ifru_mtu;
    }

    return (res);
}

int ml_network_interface_add_route(const char *name_p,
                                   const char *ip_address_p)
{
    struct ifreq ifreq;
    int res;
    int netfd;
    struct rtentry route;
    struct sockaddr_in *addr_p;

    res = -EGENERAL;
    netfd = net_open(name_p, &ifreq);

    if (netfd != -1) {
        memset(&route, 0, sizeof(route));
        addr_p = (struct sockaddr_in*)&route.rt_gateway;
        addr_p->sin_family = AF_INET;
        addr_p->sin_addr.s_addr = inet_addr(ip_address_p);
        addr_p = (struct sockaddr_in*)&route.rt_dst;
        addr_p->sin_family = AF_INET;
        addr_p->sin_addr.s_addr = INADDR_ANY;
        addr_p = (struct sockaddr_in*)&route.rt_genmask;
        addr_p->sin_family = AF_INET;
        addr_p->sin_addr.s_addr = INADDR_ANY;
        route.rt_flags = (RTF_UP | RTF_GATEWAY);
        res = ml_ioctl(netfd, SIOCADDRT, &route);

        /* Route already exists? */
        if (res == -1) {
            if (errno == EEXIST) {
                res = 0;
            } else {
                res = -errno;
            }
        }

        net_close(netfd);
    }

    return (res);
}

int ml_network_interface_link_configure(const char *name_p,
                                        int speed,
                                        int duplex,
                                        int autoneg)
{
    struct ethtool_cmd settings;
    struct ifreq ifreq;
    int res;
    int netfd;

    netfd = net_open(name_p, &ifreq);

    if (netfd == -1) {
        return (-errno);
    }

    /* Get current settings. */
    settings.cmd = ETHTOOL_GSET;
    ifreq.ifr_data = (char *)&settings;

    res = ioctl(netfd, SIOCETHTOOL, &ifreq);

    if (res == -1) {
        res = -errno;

        goto out;
    }

    /* Overwrite requested settings. Leave the rest unomdified. */
    settings.cmd = ETHTOOL_SSET;

    ml_info("Setting speed %d, duplex %d and autoneg %d.",
            speed,
            duplex,
            autoneg);

    if (speed != -1) {
        ethtool_cmd_speed_set(&settings, speed);
    }

    if (duplex != -1) {
        settings.duplex = duplex;
    }

    if (autoneg != -1) {
        settings.autoneg = autoneg;
    }

    ifreq.ifr_data = (char *)&settings;

    res = ioctl(netfd, SIOCETHTOOL, &ifreq);

    if (res == -1) {
        res = -errno;

        goto out;
    }

    net_close(netfd);

    return (0);

 out:
    net_close(netfd);

    return (res);
}

int ml_network_filter_ipv4_set(const struct ipt_replace *filter_p)
{
    return (set_filter(AF_INET,
                       IPT_SO_SET_REPLACE,
                       filter_p,
                       filter_p->size + sizeof(*filter_p)));
}

int ml_network_filter_ipv6_set(const struct ip6t_replace *filter_p)
{
    return (set_filter(AF_INET6,
                       IP6T_SO_SET_REPLACE,
                       filter_p,
                       filter_p->size + sizeof(*filter_p)));
}

struct ipt_get_entries *ml_network_filter_ipv4_get(const char *table_p,
                                                   struct ipt_getinfo *info_p)
{
    int res;
    struct ipt_get_entries *entries_p;
    socklen_t size;

    res = get_info(table_p, info_p);

    if (res != 0) {
        return (NULL);
    }

    size = sizeof(*entries_p) + info_p->size;
    entries_p = malloc(size);

    if (entries_p == NULL) {
        return (NULL);
    }

    strcpy(&entries_p->name[0], table_p);
    entries_p->size = info_p->size;
    res = get_filter(AF_INET, IPT_SO_GET_ENTRIES, entries_p, &size);

    if (res != 0) {
        free(entries_p);
        entries_p = NULL;
    }

    return (entries_p);
}

struct ip6t_get_entries *ml_network_filter_ipv6_get(const char *table_p,
                                                    struct ip6t_getinfo *info_p)
{
    int res;
    struct ip6t_get_entries *entries_p;
    socklen_t size;

    res = get_info_ipv6(table_p, info_p);

    if (res != 0) {
        return (NULL);
    }

    size = sizeof(*entries_p) + info_p->size;
    entries_p = malloc(size);

    if (entries_p == NULL) {
        return (NULL);
    }

    strcpy(&entries_p->name[0], table_p);
    entries_p->size = info_p->size;
    res = get_filter(AF_INET6, IP6T_SO_GET_ENTRIES, entries_p, &size);

    if (res != 0) {
        free(entries_p);
        entries_p = NULL;
    }

    return (entries_p);
}

void ml_network_filter_ipv4_log(const char *table_p)
{
    struct ipt_get_entries *entries_p;
    int i;
    char address[32];
    char mask[32];
    struct ipt_entry *entry_p;
    char *entry_table_p;
    size_t offset;
    struct xt_entry_target *target_p;
    int verdict;
    struct ipt_getinfo info;

    entries_p = ml_network_filter_ipv4_get(table_p, &info);

    if (entries_p == NULL) {
        return;
    }

    ml_info("network: Table: '%s'", &entries_p->name[0]);

    i = 1;
    offset = 0;
    entry_table_p = (char *)(&entries_p->entrytable[0]);

    while (offset < entries_p->size) {
        entry_p = (struct ipt_entry *)(&entry_table_p[offset]);
        target_p = ipt_get_target(entry_p);

        ml_info("network:   Entry %d:", i);
        ml_info("network:     FromIp:      %s/%s",
                format_ipv4(&address[0], entry_p->ip.src.s_addr),
                format_ipv4(&mask[0], entry_p->ip.smsk.s_addr));
        ml_info("network:     ToIp:        %s/%s",
                format_ipv4(&address[0], entry_p->ip.dst.s_addr),
                format_ipv4(&mask[0], entry_p->ip.dmsk.s_addr));
        ml_info("network:     FromIf:      '%s'", &entry_p->ip.iniface[0]);
        ml_info("network:     ToIf:        '%s'", &entry_p->ip.outiface[0]);
        ml_info("network:     Protocol:    %u", entry_p->ip.proto);
        ml_info("network:     Flags:       0x%02x", entry_p->ip.flags);
        ml_info("network:     Invflags:    0x%02x", entry_p->ip.invflags);
        ml_info("network:     NrOfPackets: %llu",
                (unsigned long long)entry_p->counters.pcnt);
        ml_info("network:     NrOfBytes:   %llu",
                (unsigned long long)entry_p->counters.bcnt);
        ml_info("network:     Cache:       0x%08x", entry_p->nfcache);
        ml_info("network:     Target:      '%s'", &target_p->u.user.name[0]);

        if (strcmp(&target_p->u.user.name[0], XT_STANDARD_TARGET) == 0) {
            verdict = ((struct xt_standard_target *)target_p)->verdict;

            if (verdict < 0) {
                if (verdict == VERDICT_ACCEPT) {
                    ml_info("network:     Verdict:     ACCEPT");
                } else if (verdict == VERDICT_DROP) {
                    ml_info("network:     Verdict:     DROP");
                } else if (verdict == VERDICT_QUEUE) {
                    ml_info("network:     Verdict:     QUEUE");
                } else if (verdict == XT_RETURN) {
                    ml_info("network:     Verdict:     RETURN");
                } else {
                    ml_info("network:     Verdict:     UNKNOWN");
                }
            } else {
                ml_info("network:     Verdict:     %d", verdict);
            }
        } else if (strcmp(&target_p->u.user.name[0], XT_ERROR_TARGET) == 0) {
            ml_info("network:     Error:       '%s'",
                    &((struct xt_error_target *)target_p)->errorname[0]);
        }

        offset += entry_p->next_offset;
        i++;
    }

    free(entries_p);
}

void ml_network_filter_ipv6_log(const char *table_p)
{
    struct ip6t_get_entries *entries_p;
    struct ip6t_getinfo info;

    entries_p = ml_network_filter_ipv6_get(table_p, &info);

    if (entries_p == NULL) {
        return;
    }

    ml_info("network: ml_network_filter_ipv6_log is not implemented.");

    free(entries_p);
}

int ml_network_filter_ipv4_accept_all(void)
{
    return (filter_apply_all(VERDICT_ACCEPT));
}

int ml_network_filter_ipv4_drop_all(void)
{
    return (filter_apply_all(VERDICT_DROP));
}
