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

#include <unistd.h>
#include <stddef.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include "nala.h"
#include "ml/ml.h"

static int ifconfig_handle;
static int route_handle;
static int ethtool_handle;

static void gset_in_assert(const void *actual_p,
                           const void *expected_p,
                           size_t size)
{
    const struct ifreq *actual_ifreq_p;
    const struct ethtool_cmd *actual_settings_p;
    const struct ifreq *expected_ifreq_p;
    const struct ethtool_cmd *expected_settings_p;

    ASSERT_EQ(size, sizeof(*actual_ifreq_p));

    actual_ifreq_p = actual_p;
    actual_settings_p = (const struct ethtool_cmd *)actual_ifreq_p->ifr_data;
    expected_ifreq_p = expected_p;
    expected_settings_p = (const struct ethtool_cmd *)expected_ifreq_p->ifr_data;

    ASSERT_EQ(&actual_ifreq_p->ifr_name[0], &expected_ifreq_p->ifr_name[0]);
    ASSERT_EQ(actual_settings_p->cmd, expected_settings_p->cmd);
}

static void gset_out_copy(void *dst_p,
                          const void *src_p,
                          size_t size)
{
    struct ifreq *dst_ifreq_p;
    struct ethtool_cmd *dst_settings_p;
    const struct ifreq *src_ifreq_p;
    const struct ethtool_cmd *src_settings_p;

    ASSERT_EQ(size, sizeof(*dst_ifreq_p));

    dst_ifreq_p = dst_p;
    dst_settings_p = (struct ethtool_cmd *)dst_ifreq_p->ifr_data;
    src_ifreq_p = src_p;
    src_settings_p = (const struct ethtool_cmd *)src_ifreq_p->ifr_data;

    memcpy(dst_settings_p, src_settings_p, sizeof(*dst_settings_p));
}

static void sset_in_assert(const void *actual_p,
                           const void *expected_p,
                           size_t size)
{
    const struct ifreq *actual_ifreq_p;
    const struct ethtool_cmd *actual_settings_p;
    const struct ifreq *expected_ifreq_p;
    const struct ethtool_cmd *expected_settings_p;

    ASSERT_EQ(size, sizeof(*actual_ifreq_p));

    actual_ifreq_p = actual_p;
    actual_settings_p = (const struct ethtool_cmd *)actual_ifreq_p->ifr_data;
    expected_ifreq_p = expected_p;
    expected_settings_p = (const struct ethtool_cmd *)expected_ifreq_p->ifr_data;

    ASSERT_EQ(&actual_ifreq_p->ifr_name[0], &expected_ifreq_p->ifr_name[0]);
    ASSERT_EQ(actual_settings_p->cmd, expected_settings_p->cmd);
    ASSERT_EQ(actual_settings_p->speed, expected_settings_p->speed);
    ASSERT_EQ(actual_settings_p->duplex, expected_settings_p->duplex);
    ASSERT_EQ(actual_settings_p->autoneg, expected_settings_p->autoneg);
}

static void mock_prepare_network_interface_link(struct ethtool_cmd *settings_gin_p,
                                                struct ethtool_cmd *settings_gout_p,
                                                struct ethtool_cmd *settings_sin_p,
                                                int sspeed,
                                                int sduplex,
                                                int sautoneg,
                                                int gspeed,
                                                int gduplex,
                                                int gautoneg)
{
    int fd;
    struct ifreq ifreq;

    fd = 7;
    socket_mock_once(AF_INET, SOCK_DGRAM, 0, fd);

    /* Get current settings. */
    memset(&ifreq, 0, sizeof(ifreq));
    strcpy(&ifreq.ifr_name[0], "eth1");
    ioctl_mock_once(fd, SIOCETHTOOL, 0, "%p");

    ifreq.ifr_data = (char *)settings_gin_p;
    settings_gin_p->cmd = ETHTOOL_GSET;
    ioctl_mock_set_va_arg_in_at(0, &ifreq, sizeof(ifreq));
    ioctl_mock_set_va_arg_in_assert_at(0, gset_in_assert);

    ifreq.ifr_data = (char *)settings_gout_p;
    settings_gout_p->cmd = ETHTOOL_GSET;
    settings_gout_p->speed = gspeed;
    settings_gout_p->speed_hi = (gspeed >> 16);
    settings_gout_p->duplex = gduplex;
    settings_gout_p->autoneg = gautoneg;
    ioctl_mock_set_va_arg_out_at(0, &ifreq, sizeof(ifreq));
    ioctl_mock_set_va_arg_out_copy_at(0, gset_out_copy);

    /* Set new settings. */
    if (settings_sin_p != NULL) {
        ifreq.ifr_data = (char *)settings_sin_p;
        settings_sin_p->cmd = ETHTOOL_SSET;
        settings_sin_p->speed = sspeed;
        settings_sin_p->duplex = sduplex;
        settings_sin_p->autoneg = sautoneg;
        ioctl_mock_once(fd, SIOCETHTOOL, 0, "%p");
        ioctl_mock_set_va_arg_in_at(0, &ifreq, sizeof(ifreq));
        ioctl_mock_set_va_arg_in_assert_at(0, sset_in_assert);
    }

    close_mock_once(fd, 0);
}

static void mock_ioctl_ifreq_ok(int fd,
                                unsigned long request,
                                struct ifreq *ifreq_p)
{
    ioctl_mock_once(fd, request, 0, "%p");
    ioctl_mock_set_va_arg_in_at(0, ifreq_p, sizeof(*ifreq_p));
}

static void mock_push_ml_network_init(void)
{
    ifconfig_handle = ml_shell_register_command_mock_once(
        "ifconfig",
        "Network interface management.");
    route_handle = ml_shell_register_command_mock_once(
        "route",
        "Network routing.");
    ethtool_handle = ml_shell_register_command_mock_once(
        "ethtool",
        "Ethernet link settings.");
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

static void mock_push_configure(const char *name_p)
{
    int fd;
    struct ifreq ifreq;

    fd = 5;
    socket_mock_once(AF_INET, SOCK_DGRAM, 0, fd);
    memset(&ifreq, 0, sizeof(ifreq));
    strcpy(&ifreq.ifr_name[0], name_p);
    create_address_request(&ifreq, "192.168.0.4");
    mock_ioctl_ifreq_ok(fd, SIOCSIFADDR, &ifreq);
    create_address_request(&ifreq, "255.255.255.0");
    mock_ioctl_ifreq_ok(fd, SIOCSIFNETMASK, &ifreq);
    ifreq.ifr_ifru.ifru_mtu = 1500;
    mock_ioctl_ifreq_ok(fd, SIOCSIFMTU, &ifreq);
    close_mock_once(fd, 0);
}

static void mock_push_up(const char *name_p)
{
    int fd;
    struct ifreq ifreq;

    fd = 5;
    socket_mock_once(AF_INET, SOCK_DGRAM, 0, fd);
    memset(&ifreq, 0, sizeof(ifreq));
    strcpy(&ifreq.ifr_name[0], name_p);
    mock_ioctl_ifreq_ok(fd, SIOCGIFFLAGS, &ifreq);
    ifreq.ifr_flags = IFF_UP;
    mock_ioctl_ifreq_ok(fd, SIOCSIFFLAGS, &ifreq);
    close_mock_once(fd, 0);
}

static void mock_push_down(const char *name_p)
{
    int fd;
    struct ifreq ifreq;

    fd = 5;
    socket_mock_once(AF_INET, SOCK_DGRAM, 0, fd);
    memset(&ifreq, 0, sizeof(ifreq));
    strcpy(&ifreq.ifr_name[0], name_p);
    mock_ioctl_ifreq_ok(fd, SIOCGIFFLAGS, &ifreq);
    mock_ioctl_ifreq_ok(fd, SIOCSIFFLAGS, &ifreq);
    close_mock_once(fd, 0);
}

static void mock_push_ioctl_get(const char *name_p,
                                unsigned long request,
                                struct ifreq *ifreq_out_p,
                                int res)
{
    int fd;
    struct ifreq ifreq_in;

    fd = 5;
    socket_mock_once(AF_INET, SOCK_DGRAM, 0, fd);
    memset(&ifreq_in, 0, sizeof(ifreq_in));
    strcpy(&ifreq_in.ifr_name[0], name_p);
    memcpy(&ifreq_out_p->ifr_name,
           &ifreq_in.ifr_name,
           sizeof(ifreq_out_p->ifr_name));
    ioctl_mock_once(fd, request, res, "%p");
    ioctl_mock_set_va_arg_in_at(0, &ifreq_in, sizeof(ifreq_in));
    ioctl_mock_set_va_arg_out_at(0, ifreq_out_p, sizeof(*ifreq_out_p));
    close_mock_once(fd, 0);
}

static void mock_push_ml_network_interface_index(const char *name_p,
                                                 int index,
                                                 int res)
{
    struct ifreq ifreq_out;

    memset(&ifreq_out, 0, sizeof(ifreq_out));
    ifreq_out.ifr_ifindex = index;
    mock_push_ioctl_get(name_p, SIOCGIFINDEX, &ifreq_out, res);
}

static void mock_push_ml_network_interface_mac_address(const char *name_p,
                                                       uint8_t *mac_address_p,
                                                       int res)
{
    struct ifreq ifreq_out;

    memset(&ifreq_out, 0, sizeof(ifreq_out));
    memcpy(&ifreq_out.ifr_hwaddr.sa_data[0], mac_address_p, 6);
    mock_push_ioctl_get(name_p, SIOCGIFHWADDR, &ifreq_out, res);
}

static void mock_push_ml_network_interface_mtu(const char *name_p,
                                               int mtu,
                                               int res)
{
    struct ifreq ifreq_out;

    memset(&ifreq_out, 0, sizeof(ifreq_out));
    ifreq_out.ifr_ifru.ifru_mtu = mtu;
    mock_push_ioctl_get(name_p, SIOCGIFMTU, &ifreq_out, res);
}

static void mock_push_ml_network_interface_ip_address(const char *name_p,
                                                      struct in_addr *ip_address_p,
                                                      int res)
{
    struct ifreq ifreq_out;

    memset(&ifreq_out, 0, sizeof(ifreq_out));
    ((struct sockaddr_in *)&ifreq_out.ifr_addr)->sin_addr = *ip_address_p;
    mock_push_ioctl_get(name_p, SIOCGIFADDR, &ifreq_out, res);
}

TEST(network_interface_configure)
{
    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    mock_push_configure("eth0");

    ASSERT_EQ(ml_network_interface_configure("eth0",
                                             "192.168.0.4",
                                             "255.255.255.0",
                                             1500), 0);
}

TEST(network_interface_up)
{
    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    mock_push_up("eth0");

    ASSERT_EQ(ml_network_interface_up("eth0"), 0);
}

TEST(network_interface_down)
{
    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    mock_push_down("eth0");

    ASSERT_EQ(ml_network_interface_down("eth0"), 0);
}

TEST(command_ifconfig_no_args)
{
    struct nala_ml_shell_register_command_params_t *params_p;
    const char *argv[] = { "ifconfig" };

    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    params_p = ml_shell_register_command_mock_get_params_in(ifconfig_handle);

    CAPTURE_OUTPUT(output, errput) {
        ASSERT_EQ(params_p->callback(membersof(argv), argv, stdout), -EINVAL);
    }

    ASSERT_EQ(output,
              "Usage: ifconfig <interface>\n"
              "       ifconfig <interface> up/down\n"
              "       ifconfig <interface> <ip-address> <netmask> <mtu>\n");
}

TEST(command_ifconfig_configure)
{
    struct nala_ml_shell_register_command_params_t *params_p;
    const char *argv[] = {
        "ifconfig", "eth2", "192.168.0.4", "255.255.255.0", "1500"
    };

    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    params_p = ml_shell_register_command_mock_get_params_in(ifconfig_handle);

    mock_push_configure("eth2");

    CAPTURE_OUTPUT(output, errput) {
        ASSERT_EQ(params_p->callback(membersof(argv), argv, stdout), 0);
    }

    ASSERT_EQ(output, "");
}

TEST(command_ifconfig_up)
{
    struct nala_ml_shell_register_command_params_t *params_p;
    const char *argv[] = { "ifconfig", "eth2", "up" };

    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    params_p = ml_shell_register_command_mock_get_params_in(ifconfig_handle);

    mock_push_up("eth2");

    CAPTURE_OUTPUT(output, errput) {
        ASSERT_EQ(params_p->callback(membersof(argv), argv, stdout), 0);
    }

    ASSERT_EQ(output, "");
}

TEST(command_ifconfig_down)
{
    struct nala_ml_shell_register_command_params_t *params_p;
    const char *argv[] = { "ifconfig", "eth1", "down" };

    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    params_p = ml_shell_register_command_mock_get_params_in(ifconfig_handle);

    mock_push_down("eth1");

    CAPTURE_OUTPUT(output, errput) {
        ASSERT_EQ(params_p->callback(membersof(argv), argv, stdout), 0);
    }

    ASSERT_EQ(output, "");
}

TEST(command_ifconfig_foobar)
{
    struct nala_ml_shell_register_command_params_t *params_p;
    const char *argv[] = { "ifconfig", "eth1", "foobar" };

    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    params_p = ml_shell_register_command_mock_get_params_in(ifconfig_handle);

    CAPTURE_OUTPUT(output, errput) {
        ASSERT_EQ(params_p->callback(membersof(argv), argv, stdout), -EINVAL);
    }

    ASSERT_EQ(output,
              "Usage: ifconfig <interface>\n"
              "       ifconfig <interface> up/down\n"
              "       ifconfig <interface> <ip-address> <netmask> <mtu>\n");
}

TEST(command_ifconfig_print)
{
    struct nala_ml_shell_register_command_params_t *params_p;
    const char *argv[] = { "ifconfig", "eth1" };
    struct in_addr ip_address;
    uint8_t mac_address[6];

    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    params_p = ml_shell_register_command_mock_get_params_in(ifconfig_handle);

    inet_aton("1.2.3.5", &ip_address);
    mock_push_ml_network_interface_ip_address("eth1", &ip_address, 0);
    mac_address[0] = 5;
    mac_address[1] = 6;
    mac_address[2] = 7;
    mac_address[3] = 8;
    mac_address[4] = 9;
    mac_address[5] = 10;
    mock_push_ml_network_interface_mac_address("eth1", &mac_address[0], 0);
    mock_push_ml_network_interface_mtu("eth1", 1500, 0);
    mock_push_ml_network_interface_index("eth1", 5, 0);

    CAPTURE_OUTPUT(output, errput) {
        ASSERT_EQ(params_p->callback(membersof(argv), argv, stdout), 0);
    }

    ASSERT_EQ(output,
              "IPAddress:  1.2.3.5\n"
              "MACAddress: 05:06:07:08:09:0a\n"
              "MTU:        1500\n"
              "Index:      5\n");
}

TEST(command_ifconfig_print_failures)
{
    struct nala_ml_shell_register_command_params_t *params_p;
    const char *argv[] = { "ifconfig", "eth1" };
    struct in_addr ip_address;
    uint8_t mac_address[6];

    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    params_p = ml_shell_register_command_mock_get_params_in(ifconfig_handle);

    inet_aton("1.2.3.5", &ip_address);
    mock_push_ml_network_interface_ip_address("eth1", &ip_address, -1);
    mac_address[0] = 5;
    mac_address[1] = 6;
    mac_address[2] = 7;
    mac_address[3] = 8;
    mac_address[4] = 9;
    mac_address[5] = 10;
    mock_push_ml_network_interface_mac_address("eth1", &mac_address[0], -1);
    mock_push_ml_network_interface_mtu("eth1", 1500, -1);
    mock_push_ml_network_interface_index("eth1", 5, -1);

    CAPTURE_OUTPUT(output, errput) {
        ASSERT_EQ(params_p->callback(membersof(argv), argv, stdout), 0);
    }

    ASSERT_EQ(output,
              "IPAddress:  failure\n"
              "MACAddress: failure\n"
              "MTU:        failure\n"
              "Index:      failure\n");
}

TEST(command_ethtool_no_args)
{
    struct nala_ml_shell_register_command_params_t *params_p;
    const char *argv[] = { "ethtool" };

    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    params_p = ml_shell_register_command_mock_get_params_in(ethtool_handle);

    CAPTURE_OUTPUT(output, errput) {
        ASSERT_EQ(params_p->callback(membersof(argv), argv, stdout), -EINVAL);
    }

    ASSERT_EQ(output,
              "Usage: ethtool <interface>\n"
              "       ethtool <interface> <speed> <duplex> <autoneg>\n"
              "         where\n"
              "           <speed> is the speed in Mbps or -\n"
              "           <duplex> is half, full or -\n"
              "           <autoneg> is on, off or -\n");
}

TEST(command_ethtool_configure)
{
    struct nala_ml_shell_register_command_params_t *params_p;
    const char *argv[] = { "ethtool", "eth1", "100", "full", "off" };

    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();
    ml_network_interface_link_configure_mock_once("eth1",
                                                  100,
                                                  DUPLEX_FULL,
                                                  AUTONEG_DISABLE,
                                                  0);

    CAPTURE_OUTPUT(output, errput) {
        params_p = ml_shell_register_command_mock_get_params_in(ethtool_handle);
        ASSERT_EQ(params_p->callback(membersof(argv), argv, stdout), 0);
    }

    ASSERT_EQ(output, "");
}

TEST(command_ethtool_configure_no_changes)
{
    struct nala_ml_shell_register_command_params_t *params_p;
    const char *argv[] = { "ethtool", "eth1", "-", "-", "-" };
    struct ethtool_cmd settings_gin;
    struct ethtool_cmd settings_gout;
    struct ethtool_cmd settings_sin;

    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    mock_prepare_network_interface_link(&settings_gin,
                                        &settings_gout,
                                        &settings_sin,
                                        1000,
                                        DUPLEX_HALF,
                                        AUTONEG_ENABLE,
                                        1000,
                                        DUPLEX_HALF,
                                        AUTONEG_ENABLE);

    CAPTURE_OUTPUT(output, errput) {
        params_p = ml_shell_register_command_mock_get_params_in(ethtool_handle);
        ASSERT_EQ(params_p->callback(membersof(argv), argv, stdout), 0);
    }

    ASSERT_EQ(output, "");
}

TEST(command_ethtool_print)
{
    struct nala_ml_shell_register_command_params_t *params_p;
    const char *argv[] = { "ethtool", "eth1" };
    struct ethtool_cmd settings_gin;
    struct ethtool_cmd settings_gout;

    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    mock_prepare_network_interface_link(&settings_gin,
                                        &settings_gout,
                                        NULL,
                                        0,
                                        0,
                                        0,
                                        1000,
                                        DUPLEX_HALF,
                                        AUTONEG_ENABLE);

    CAPTURE_OUTPUT(output, errput) {
        params_p = ml_shell_register_command_mock_get_params_in(ethtool_handle);
        ASSERT_EQ(params_p->callback(membersof(argv), argv, stdout), 0);
    }

    ASSERT_EQ(output,
              "Speed:           1000 Mbps\n"
              "Duplex:          half\n"
              "Autonegotiation: on\n");
}

TEST(command_ethtool_print_link_down)
{
    struct nala_ml_shell_register_command_params_t *params_p;
    const char *argv[] = { "ethtool", "eth1" };
    struct ethtool_cmd settings_gin;
    struct ethtool_cmd settings_gout;

    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    mock_prepare_network_interface_link(&settings_gin,
                                        &settings_gout,
                                        NULL,
                                        0,
                                        0,
                                        0,
                                        SPEED_UNKNOWN,
                                        DUPLEX_UNKNOWN,
                                        AUTONEG_ENABLE);

    CAPTURE_OUTPUT(output, errput) {
        params_p = ml_shell_register_command_mock_get_params_in(ethtool_handle);
        ASSERT_EQ(params_p->callback(membersof(argv), argv, stdout), 0);
    }

    ASSERT_EQ(output,
              "Speed:           - Mbps\n"
              "Duplex:          unknown\n"
              "Autonegotiation: on\n");
}

static void mock_prepare_get_info(struct ipt_getinfo *info_p)
{
    int fd;

    fd = 4;
    socket_mock_once(AF_INET, SOCK_RAW, IPPROTO_RAW, fd);
    getsockopt_mock_once(fd, SOL_IP, IPT_SO_GET_INFO, 0);
    getsockopt_mock_set_optval_out(info_p, sizeof(*info_p));
    close_mock_once(fd, 0);
}

TEST(filter_ipv4_set_ok)
{
    int fd;
    struct ipt_replace filter;

    fd = 4;
    memset(&filter, 0, sizeof(filter));
    socket_mock_once(AF_INET, SOCK_RAW, IPPROTO_RAW, fd);
    setsockopt_mock_once(fd, SOL_IP, IPT_SO_SET_REPLACE, 0, 0);
    setsockopt_mock_set_optval_in_pointer(&filter);
    setsockopt_mock_ignore_optlen_in();
    close_mock_once(fd, 0);

    ASSERT_EQ(ml_network_filter_ipv4_set(&filter), 0);
}

TEST(filter_ipv6_set_ok)
{
    int fd;
    struct ip6t_replace filter;

    fd = 5;
    memset(&filter, 0, sizeof(filter));
    socket_mock_once(AF_INET6, SOCK_RAW, IPPROTO_RAW, fd);
    setsockopt_mock_once(fd, SOL_IP, IP6T_SO_SET_REPLACE, 0, 0);
    setsockopt_mock_set_optval_in_pointer(&filter);
    setsockopt_mock_ignore_optlen_in();
    close_mock_once(fd, 0);

    ASSERT_EQ(ml_network_filter_ipv6_set(&filter), 0);
}

TEST(filter_set_setsockopt_error)
{
    int fd;
    struct ipt_replace filter;

    fd = 6;
    memset(&filter, 0, sizeof(filter));
    socket_mock_once(AF_INET, SOCK_RAW, IPPROTO_RAW, fd);
    setsockopt_mock_once(fd, SOL_IP, IPT_SO_SET_REPLACE, 0, -1);
    setsockopt_mock_ignore_optlen_in();
    setsockopt_mock_set_errno(EACCES);
    close_mock_once(fd, 0);

    ASSERT_EQ(ml_network_filter_ipv4_set(&filter), -EACCES);
}

TEST(filter_set_socket_error)
{
    struct ipt_replace filter;

    memset(&filter, 0, sizeof(filter));
    socket_mock_once(AF_INET, SOCK_RAW, IPPROTO_RAW, -1);
    socket_mock_set_errno(EACCES);
    close_mock_none();

    ASSERT_EQ(ml_network_filter_ipv4_set(&filter), -EACCES);
}

TEST(filter_ipv4_get_ok)
{
    int fd;
    struct ipt_getinfo info;
    struct ipt_get_entries entries;
    struct ipt_get_entries *entries_p;

    /* Get info. */
    memset(&info, 0, sizeof(info));
    info.size = sizeof(entries);
    mock_prepare_get_info(&info);

    /* Get entries. */
    fd = 5;
    memset(&entries, 0, sizeof(entries));
    socket_mock_once(AF_INET, SOCK_RAW, IPPROTO_RAW, fd);
    getsockopt_mock_once(fd, SOL_IP, IPT_SO_GET_ENTRIES, 0);
    getsockopt_mock_set_optval_out(&entries, sizeof(entries));
    close_mock_once(fd, 0);

    memset(&info, 0, sizeof(info));
    entries_p = ml_network_filter_ipv4_get("filter", &info);
    ASSERT_NE(entries_p, NULL);
    ASSERT_EQ(info.size, sizeof(entries));
    free(entries_p);
}

TEST(filter_ipv6_get_ok)
{
    int fd;
    struct ip6t_getinfo info;
    struct ip6t_get_entries entries;
    struct ip6t_get_entries *entries_p;

    /* Get info. */
    fd = 4;
    memset(&info, 0, sizeof(info));
    info.size = sizeof(entries);
    socket_mock_once(AF_INET6, SOCK_RAW, IPPROTO_RAW, fd);
    getsockopt_mock_once(fd, SOL_IP, IP6T_SO_GET_INFO, 0);
    getsockopt_mock_set_optval_out(&info, sizeof(info));
    close_mock_once(fd, 0);

    /* Get entries. */
    fd = 5;
    memset(&entries, 0, sizeof(entries));
    socket_mock_once(AF_INET6, SOCK_RAW, IPPROTO_RAW, fd);
    getsockopt_mock_once(fd, SOL_IP, IP6T_SO_GET_ENTRIES, 0);
    getsockopt_mock_set_optval_out(&entries, sizeof(entries));
    close_mock_once(fd, 0);

    memset(&info, 0, sizeof(info));
    entries_p = ml_network_filter_ipv6_get("filter", &info);
    ASSERT_NE(entries_p, NULL);
    ASSERT_EQ(info.size, sizeof(entries));
    free(entries_p);
}

TEST(filter_ipv4_get_get_info_socket_error)
{
    struct ipt_getinfo info;
    struct ipt_get_entries *entries_p;

    socket_mock_once(AF_INET, SOCK_RAW, IPPROTO_RAW, -1);
    close_mock_none();

    entries_p = ml_network_filter_ipv4_get("filter", &info);
    ASSERT_EQ(entries_p, NULL);
}

TEST(filter_ipv4_get_get_entries_socket_error)
{
    struct ipt_getinfo info;
    struct ipt_get_entries *entries_p;

    /* Get info. */
    memset(&info, 0, sizeof(info));
    info.size = sizeof(*entries_p);
    mock_prepare_get_info(&info);

    /* Get entries. */
    socket_mock_once(AF_INET, SOCK_RAW, IPPROTO_RAW, -1);

    memset(&info, 0, sizeof(info));
    entries_p = ml_network_filter_ipv4_get("filter", &info);
    ASSERT_EQ(entries_p, NULL);
}

TEST(filter_ipv4_get_get_entries_malloc_error)
{
    struct ipt_getinfo info;
    struct ipt_get_entries *entries_p;

    /* Get info. */
    memset(&info, 0, sizeof(info));
    mock_prepare_get_info(&info);

    /* Malloc fails. */
    malloc_mock_once(sizeof(*entries_p), NULL);

    memset(&info, 0, sizeof(info));
    entries_p = ml_network_filter_ipv4_get("filter", &info);
    ASSERT_EQ(entries_p, NULL);
}

struct entry_t {
    struct ipt_entry entry;
    struct xt_entry_target target;
    int pos;
};

TEST(filter_ipv4_log)
{
    int fd;
    struct ipt_getinfo info;
    struct ipt_get_entries *entries_p;
    struct ipt_entry *entry_p;
    char *entry_table_p;
    struct xt_entry_target *target_p;
    int *pos_p;
    char *message_p;
    size_t entries_size;
    size_t size;

    /* Get info. */
    entries_size = 7 * (sizeof(*entry_p) + sizeof(*target_p) + 2 * sizeof(int));
    memset(&info, 0, sizeof(info));
    info.size = entries_size;
    mock_prepare_get_info(&info);

    /* Get entries. */
    fd = 5;
    size = sizeof(*entries_p) + entries_size;
    entries_p = nala_alloc(size);
    memset(entries_p, 0, size);
    strcpy(&entries_p->name[0], "filter");
    entries_p->size = entries_size;

    /* Entry 1. */
    entry_table_p = (char *)(&entries_p->entrytable[0]);
    entry_p = (struct ipt_entry *)entry_table_p;
    entry_p->ip.src.s_addr = 0x11223344;
    entry_p->ip.smsk.s_addr = 0xffffff00;
    entry_p->ip.dst.s_addr = 0x55667788;
    entry_p->ip.dmsk.s_addr = 0xffff0000;
    strcpy(&entry_p->ip.iniface[0], "");
    strcpy(&entry_p->ip.outiface[0], "");
    entry_p->ip.proto = 0;
    entry_p->ip.flags = 0;
    entry_p->ip.invflags = 0;
    entry_p->counters.pcnt = 1;
    entry_p->counters.bcnt = 2;
    entry_p->nfcache = 0;
    entry_p->target_offset = sizeof(*entry_p);
    entry_p->next_offset = (entry_p->target_offset
                            + sizeof(*target_p)
                            + 2 * sizeof(int));
    target_p = (struct xt_entry_target *)&entry_table_p[entry_p->target_offset];
    strcpy(&target_p->u.user.name[0], "");
    pos_p = (int *)(&target_p->data[0]);
    *pos_p = -NF_ACCEPT - 1;

    /* Entry 2. */
    entry_table_p += entry_p->next_offset;
    entry_p = (struct ipt_entry *)entry_table_p;
    entry_p->ip.src.s_addr = 0x00000000;
    entry_p->ip.smsk.s_addr = 0x00000000;
    entry_p->ip.dst.s_addr = 0x00000000;
    entry_p->ip.dmsk.s_addr = 0x00000000;
    strcpy(&entry_p->ip.iniface[0], "eth1");
    strcpy(&entry_p->ip.outiface[0], "eth2");
    entry_p->ip.proto = 7;
    entry_p->ip.flags = 1;
    entry_p->ip.invflags = 2;
    entry_p->counters.pcnt = 3;
    entry_p->counters.bcnt = 2;
    entry_p->nfcache = 1;
    entry_p->target_offset = sizeof(*entry_p);
    entry_p->next_offset = (entry_p->target_offset
                            + sizeof(*target_p)
                            + 2 * sizeof(int));
    target_p = (struct xt_entry_target *)&entry_table_p[entry_p->target_offset];
    strcpy(&target_p->u.user.name[0], "");
    pos_p = (int *)(&target_p->data[0]);
    *pos_p = -NF_DROP - 1;

    /* Entry 3. */
    entry_table_p += entry_p->next_offset;
    entry_p = (struct ipt_entry *)entry_table_p;
    entry_p->ip.src.s_addr = 0x00000000;
    entry_p->ip.smsk.s_addr = 0xffffff00;
    entry_p->ip.dst.s_addr = 0x00000000;
    entry_p->ip.dmsk.s_addr = 0xffff0000;
    strcpy(&entry_p->ip.iniface[0], "");
    strcpy(&entry_p->ip.outiface[0], "");
    entry_p->ip.proto = 0;
    entry_p->ip.flags = 0;
    entry_p->ip.invflags = 0;
    entry_p->counters.pcnt = 1;
    entry_p->counters.bcnt = 2;
    entry_p->nfcache = 0;
    entry_p->target_offset = sizeof(*entry_p);
    entry_p->next_offset = (entry_p->target_offset
                            + sizeof(*target_p)
                            + 2 * sizeof(int));
    target_p = (struct xt_entry_target *)&entry_table_p[entry_p->target_offset];
    strcpy(&target_p->u.user.name[0], "");
    pos_p = (int *)(&target_p->data[0]);
    *pos_p = -NF_QUEUE - 1;

    /* Entry 4. */
    entry_table_p += entry_p->next_offset;
    entry_p = (struct ipt_entry *)entry_table_p;
    entry_p->ip.src.s_addr = 0x00000000;
    entry_p->ip.smsk.s_addr = 0xffffff00;
    entry_p->ip.dst.s_addr = 0x00000000;
    entry_p->ip.dmsk.s_addr = 0xffff0000;
    strcpy(&entry_p->ip.iniface[0], "");
    strcpy(&entry_p->ip.outiface[0], "");
    entry_p->ip.proto = 0;
    entry_p->ip.flags = 0;
    entry_p->ip.invflags = 0;
    entry_p->counters.pcnt = 1;
    entry_p->counters.bcnt = 2;
    entry_p->nfcache = 0;
    entry_p->target_offset = sizeof(*entry_p);
    entry_p->next_offset = (entry_p->target_offset
                            + sizeof(*target_p)
                            + 2 * sizeof(int));
    target_p = (struct xt_entry_target *)&entry_table_p[entry_p->target_offset];
    strcpy(&target_p->u.user.name[0], "");
    pos_p = (int *)(&target_p->data[0]);
    *pos_p = XT_RETURN;

    /* Entry 5. */
    entry_table_p += entry_p->next_offset;
    entry_p = (struct ipt_entry *)entry_table_p;
    entry_p->ip.src.s_addr = 0x00000000;
    entry_p->ip.smsk.s_addr = 0xffffff00;
    entry_p->ip.dst.s_addr = 0x00000000;
    entry_p->ip.dmsk.s_addr = 0xffff0000;
    strcpy(&entry_p->ip.iniface[0], "");
    strcpy(&entry_p->ip.outiface[0], "");
    entry_p->ip.proto = 0;
    entry_p->ip.flags = 0;
    entry_p->ip.invflags = 0;
    entry_p->counters.pcnt = 1;
    entry_p->counters.bcnt = 2;
    entry_p->nfcache = 0;
    entry_p->target_offset = sizeof(*entry_p);
    entry_p->next_offset = (entry_p->target_offset
                            + sizeof(*target_p)
                            + 2 * sizeof(int));
    target_p = (struct xt_entry_target *)&entry_table_p[entry_p->target_offset];
    strcpy(&target_p->u.user.name[0], "");
    pos_p = (int *)(&target_p->data[0]);
    *pos_p = -999;

    /* Entry 6. */
    entry_table_p += entry_p->next_offset;
    entry_p = (struct ipt_entry *)entry_table_p;
    entry_p->ip.src.s_addr = 0x00000000;
    entry_p->ip.smsk.s_addr = 0xffffff00;
    entry_p->ip.dst.s_addr = 0x00000000;
    entry_p->ip.dmsk.s_addr = 0xffff0000;
    strcpy(&entry_p->ip.iniface[0], "");
    strcpy(&entry_p->ip.outiface[0], "");
    entry_p->ip.proto = 0;
    entry_p->ip.flags = 0;
    entry_p->ip.invflags = 0;
    entry_p->counters.pcnt = 1;
    entry_p->counters.bcnt = 2;
    entry_p->nfcache = 0;
    entry_p->target_offset = sizeof(*entry_p);
    entry_p->next_offset = (entry_p->target_offset
                            + sizeof(*target_p)
                            + 2 * sizeof(int));
    target_p = (struct xt_entry_target *)&entry_table_p[entry_p->target_offset];
    strcpy(&target_p->u.user.name[0], "");
    pos_p = (int *)(&target_p->data[0]);
    *pos_p = 999;

    /* Entry 7. */
    entry_table_p += entry_p->next_offset;
    entry_p = (struct ipt_entry *)entry_table_p;
    entry_p->ip.src.s_addr = 0x00000000;
    entry_p->ip.smsk.s_addr = 0xffffff00;
    entry_p->ip.dst.s_addr = 0x00000000;
    entry_p->ip.dmsk.s_addr = 0xffff0000;
    strcpy(&entry_p->ip.iniface[0], "");
    strcpy(&entry_p->ip.outiface[0], "");
    entry_p->ip.proto = 0;
    entry_p->ip.flags = 0;
    entry_p->ip.invflags = 0;
    entry_p->counters.pcnt = 0;
    entry_p->counters.bcnt = 0;
    entry_p->nfcache = 0;
    entry_p->target_offset = sizeof(*entry_p);
    entry_p->next_offset = (entry_p->target_offset
                            + sizeof(*target_p)
                            + 2 * sizeof(int));
    target_p = (struct xt_entry_target *)&entry_table_p[entry_p->target_offset];
    strcpy(&target_p->u.user.name[0], "ERROR");
    message_p = (char *)(&target_p->data[0]);
    strcpy(message_p, "ERR");

    socket_mock_once(AF_INET, SOCK_RAW, IPPROTO_RAW, fd);
    getsockopt_mock_once(fd, SOL_IP, IPT_SO_GET_ENTRIES, 0);
    getsockopt_mock_set_optval_out(entries_p, size);
    close_mock_once(fd, 0);

    ml_init();

    CAPTURE_OUTPUT(output, errput) {
        ml_network_filter_ipv4_log("filter");
    }

    ASSERT_SUBSTRING(output, "network: Table: 'filter'");
    ASSERT_SUBSTRING(output, "network:   Entry 1:");
    ASSERT_SUBSTRING(output, "network:     FromIp:      17.34.51.68/255.255.255.0");
    ASSERT_SUBSTRING(output, "network:     ToIp:        85.102.119.136/255.255.0.0");
    ASSERT_SUBSTRING(output, "network:     FromIf:      ''");
    ASSERT_SUBSTRING(output, "network:     ToIf:        ''");
    ASSERT_SUBSTRING(output, "network:     Protocol:    0");
    ASSERT_SUBSTRING(output, "network:     Flags:       0x00");
    ASSERT_SUBSTRING(output, "network:     Invflags:    0x00");
    ASSERT_SUBSTRING(output, "network:     NrOfPackets: 1");
    ASSERT_SUBSTRING(output, "network:     NrOfBytes:   2");
    ASSERT_SUBSTRING(output, "network:     Cache:       0x00000000");
    ASSERT_SUBSTRING(output, "network:     Target:      ''");
    ASSERT_SUBSTRING(output, "network:     Verdict:     ACCEPT");

    ASSERT_SUBSTRING(output, "network:   Entry 2:");
    ASSERT_SUBSTRING(output, "network:     FromIp:      0.0.0.0/0.0.0.0");
    ASSERT_SUBSTRING(output, "network:     ToIp:        0.0.0.0/0.0.0.0");
    ASSERT_SUBSTRING(output, "network:     FromIf:      'eth1'");
    ASSERT_SUBSTRING(output, "network:     ToIf:        'eth2'");
    ASSERT_SUBSTRING(output, "network:     Protocol:    7");
    ASSERT_SUBSTRING(output, "network:     Flags:       0x01");
    ASSERT_SUBSTRING(output, "network:     Invflags:    0x02");
    ASSERT_SUBSTRING(output, "network:     NrOfPackets: 3");
    ASSERT_SUBSTRING(output, "network:     NrOfBytes:   2");
    ASSERT_SUBSTRING(output, "network:     Cache:       0x00000001");
    ASSERT_SUBSTRING(output, "network:     Target:      ''");
    ASSERT_SUBSTRING(output, "network:     Verdict:     DROP");

    ASSERT_SUBSTRING(output, "network:   Entry 3:");
    ASSERT_SUBSTRING(output, "network:     FromIp:      0.0.0.0/255.255.255.0");
    ASSERT_SUBSTRING(output, "network:     ToIp:        0.0.0.0/255.255.0.0");
    ASSERT_SUBSTRING(output, "network:     NrOfPackets: 0");
    ASSERT_SUBSTRING(output, "network:     NrOfBytes:   0");
    ASSERT_SUBSTRING(output, "network:     Verdict:     QUEUE");

    ASSERT_SUBSTRING(output, "network:   Entry 4:");
    ASSERT_SUBSTRING(output, "network:     Verdict:     RETURN");

    ASSERT_SUBSTRING(output, "network:   Entry 5:");
    ASSERT_SUBSTRING(output, "network:     Verdict:     UNKNOWN");

    ASSERT_SUBSTRING(output, "network:   Entry 6:");
    ASSERT_SUBSTRING(output, "network:     Verdict:     999");

    ASSERT_SUBSTRING(output, "network:   Entry 7:");
    ASSERT_SUBSTRING(output, "network:     Target:      'ERROR'");
    ASSERT_SUBSTRING(output, "network:     Error:       'ERR'");

    ASSERT_NOT_SUBSTRING(output, "network:   Entry 0:");
    ASSERT_NOT_SUBSTRING(output, "network:   Entry 8:");
}

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

static void mock_prepare_apply_all(int verdict)
{
    int fd;
    struct replace_t replace;
    struct ipt_getinfo info;
    struct standard_entry_t *standard_p;
    struct error_entry_t *error_p;

    /* Get info with 6 entries. */
    memset(&info, 0, sizeof(info));
    info.size = (5 * sizeof(struct standard_entry_t)
                 + sizeof(struct error_entry_t));
    info.num_entries = 6;
    mock_prepare_get_info(&info);

    /* Accept all should set 4 entries (input, forward, output and
       error). */
    memset(&replace, 0, sizeof(replace));

    strcpy(&replace.header.name[0], "filter");
    replace.header.valid_hooks = 0x0e;
    replace.header.num_entries = 4;
    replace.header.size = sizeof(replace) - sizeof(replace.header);
    replace.header.hook_entry[NF_INET_LOCAL_IN] = 0;
    replace.header.hook_entry[NF_INET_FORWARD] = sizeof(replace.standard[0]);
    replace.header.hook_entry[NF_INET_LOCAL_OUT] = 2 * sizeof(replace.standard[0]);
    replace.header.underflow[NF_INET_LOCAL_IN] = 0;
    replace.header.underflow[NF_INET_FORWARD] = sizeof(replace.standard[0]);
    replace.header.underflow[NF_INET_LOCAL_OUT] = 2 * sizeof(replace.standard[0]);
    replace.header.num_counters = 6;

    standard_p = &replace.standard[0];
    standard_p->entry.target_offset = offsetof(struct standard_entry_t, standard);
    standard_p->entry.next_offset = sizeof(*standard_p);
    standard_p->standard.target.u.target_size = sizeof(standard_p->standard);
    standard_p->standard.verdict = verdict;

    standard_p = &replace.standard[1];
    standard_p->entry.target_offset = offsetof(struct standard_entry_t, standard);
    standard_p->entry.next_offset = sizeof(*standard_p);
    standard_p->standard.target.u.target_size = sizeof(standard_p->standard);
    standard_p->standard.verdict = verdict;

    standard_p = &replace.standard[2];
    standard_p->entry.target_offset = offsetof(struct standard_entry_t, standard);
    standard_p->entry.next_offset = sizeof(*standard_p);
    standard_p->standard.target.u.target_size = sizeof(standard_p->standard);
    standard_p->standard.verdict = verdict;

    error_p = &replace.error;
    error_p->entry.target_offset = offsetof(struct error_entry_t, error);
    error_p->entry.next_offset = sizeof(*error_p);
    error_p->error.target.u.user.target_size = sizeof(error_p->error);
    strcpy(&error_p->error.target.u.user.name[0], "ERROR");
    strcpy(&error_p->error.errorname[0], "ERROR");

    fd = 9;
    socket_mock_once(AF_INET, SOCK_RAW, IPPROTO_RAW, fd);
    setsockopt_mock_once(fd, SOL_IP, IPT_SO_SET_REPLACE, sizeof(replace), 0);
    setsockopt_mock_set_optval_in(&replace, sizeof(replace));
    close_mock_once(fd, 0);
}

TEST(filter_ipv4_accept_all)
{
    mock_prepare_apply_all(-NF_ACCEPT - 1);

    ASSERT_EQ(ml_network_filter_ipv4_accept_all(), 0);
}

TEST(filter_ipv4_accept_all_get_info_socket_error)
{
    socket_mock_once(AF_INET, SOCK_RAW, IPPROTO_RAW, -1);

    ASSERT_EQ(ml_network_filter_ipv4_accept_all(), -EGENERAL);
}

TEST(filter_ipv4_drop_all)
{
    mock_prepare_apply_all(-NF_DROP - 1);

    ASSERT_EQ(ml_network_filter_ipv4_drop_all(), 0);
}

TEST(network_interface_link_configure)
{
    struct ethtool_cmd settings_gin;
    struct ethtool_cmd settings_gout;
    struct ethtool_cmd settings_sin;

    mock_prepare_network_interface_link(&settings_gin,
                                        &settings_gout,
                                        &settings_sin,
                                        100,
                                        DUPLEX_FULL,
                                        AUTONEG_DISABLE,
                                        1000,
                                        DUPLEX_HALF,
                                        AUTONEG_ENABLE);

    ASSERT_EQ(ml_network_interface_link_configure("eth1",
                                                  100,
                                                  DUPLEX_FULL,
                                                  AUTONEG_DISABLE), 0);
}
