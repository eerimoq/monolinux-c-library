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

#include "narwhal.h"
#include "ml/ml.h"
#include "utils/utils.h"
#include "utils/mocks/mock_libc.h"
#include "utils/mocks/mock_ml_shell.h"
#include "utils/mocks/mock.h"

static void mock_push_ml_network_init(void)
{
    mock_push_ml_shell_register_command("ifconfig",
                                        "Network interface management.");
    mock_push_ml_shell_register_command("route",
                                        "Network routing.");
    mock_push_ml_shell_register_command("udp_send",
                                        "UDP send.");
    mock_push_ml_shell_register_command("udp_recv",
                                        "UDP receive.");
    mock_push_ml_shell_register_command("tcp_send",
                                        "TCP send.");
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
    mock_push_socket(AF_INET, SOCK_DGRAM, 0, fd);
    memset(&ifreq, 0, sizeof(ifreq));
    strcpy(&ifreq.ifr_name[0], name_p);
    create_address_request(&ifreq, "192.168.0.4");
    mock_push_ioctl_ifreq_ok(fd, SIOCSIFADDR, &ifreq);
    create_address_request(&ifreq, "255.255.255.0");
    mock_push_ioctl_ifreq_ok(fd, SIOCSIFNETMASK, &ifreq);
    mock_push_ml_close(fd, 0);
}

static void mock_push_up(const char *name_p)
{
    int fd;
    struct ifreq ifreq;

    fd = 5;
    mock_push_socket(AF_INET, SOCK_DGRAM, 0, fd);
    memset(&ifreq, 0, sizeof(ifreq));
    strcpy(&ifreq.ifr_name[0], name_p);
    mock_push_ioctl_ifreq_ok(fd, SIOCGIFFLAGS, &ifreq);
    ifreq.ifr_flags = IFF_UP;
    mock_push_ioctl_ifreq_ok(fd, SIOCSIFFLAGS, &ifreq);
    mock_push_ml_close(fd, 0);
}

static void mock_push_down(const char *name_p)
{
    int fd;
    struct ifreq ifreq;

    fd = 5;
    mock_push_socket(AF_INET, SOCK_DGRAM, 0, fd);
    memset(&ifreq, 0, sizeof(ifreq));
    strcpy(&ifreq.ifr_name[0], name_p);
    mock_push_ioctl_ifreq_ok(fd, SIOCGIFFLAGS, &ifreq);
    mock_push_ioctl_ifreq_ok(fd, SIOCSIFFLAGS, &ifreq);
    mock_push_ml_close(fd, 0);
}

static void mock_push_ioctl_get(const char *name_p,
                                unsigned long request,
                                struct ifreq *ifreq_out_p,
                                int res)
{
    int fd;
    struct ifreq ifreq_in;

    fd = 5;
    mock_push_socket(AF_INET, SOCK_DGRAM, 0, fd);
    memset(&ifreq_in, 0, sizeof(ifreq_in));
    strcpy(&ifreq_in.ifr_name[0], name_p);
    memcpy(&ifreq_out_p->ifr_name,
           &ifreq_in.ifr_name,
           sizeof(ifreq_out_p->ifr_name));
    mock_push_ioctl(fd,
                    request,
                    &ifreq_in,
                    ifreq_out_p,
                    sizeof(ifreq_in),
                    res);
    mock_push_ml_close(fd, 0);
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
                                             "255.255.255.0"), 0);
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
    ml_shell_command_callback_t command_ifconfig;
    const char *argv[] = { "ifconfig" };

    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    command_ifconfig = mock_get_callback("ifconfig");

    CAPTURE_OUTPUT(output, errput) {
        ASSERT_EQ(command_ifconfig(membersof(argv), argv), -1);
    }

    ASSERT_EQ(output,
              "ifconfig <interface>\n"
              "ifconfig <interface> up/down\n"
              "ifconfig <interface> <ip-address> <netmask>\n");
}

TEST(command_ifconfig_configure)
{
    ml_shell_command_callback_t command_ifconfig;
    const char *argv[] = { "ifconfig", "eth2", "192.168.0.4", "255.255.255.0" };

    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    command_ifconfig = mock_get_callback("ifconfig");

    mock_push_configure("eth2");

    CAPTURE_OUTPUT(output, errput) {
        ASSERT_EQ(command_ifconfig(membersof(argv), argv), 0);
    }

    ASSERT_EQ(output, "");
}

TEST(command_ifconfig_up)
{
    ml_shell_command_callback_t command_ifconfig;
    const char *argv[] = { "ifconfig", "eth2", "up" };

    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    command_ifconfig = mock_get_callback("ifconfig");

    mock_push_up("eth2");

    CAPTURE_OUTPUT(output, errput) {
        ASSERT_EQ(command_ifconfig(membersof(argv), argv), 0);
    }

    ASSERT_EQ(output, "");
}

TEST(command_ifconfig_down)
{
    ml_shell_command_callback_t command_ifconfig;
    const char *argv[] = { "ifconfig", "eth1", "down" };

    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    command_ifconfig = mock_get_callback("ifconfig");

    mock_push_down("eth1");

    CAPTURE_OUTPUT(output, errput) {
        ASSERT_EQ(command_ifconfig(membersof(argv), argv), 0);
    }

    ASSERT_EQ(output, "");
}

TEST(command_ifconfig_foobar)
{
    ml_shell_command_callback_t command_ifconfig;
    const char *argv[] = { "ifconfig", "eth1", "foobar" };

    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    command_ifconfig = mock_get_callback("ifconfig");

    CAPTURE_OUTPUT(output, errput) {
        ASSERT_EQ(command_ifconfig(membersof(argv), argv), -1);
    }

    ASSERT_EQ(output,
              "ifconfig <interface>\n"
              "ifconfig <interface> up/down\n"
              "ifconfig <interface> <ip-address> <netmask>\n");
}

TEST(command_ifconfig_print)
{
    ml_shell_command_callback_t command_ifconfig;
    const char *argv[] = { "ifconfig", "eth1" };
    struct in_addr ip_address;
    uint8_t mac_address[6];

    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    command_ifconfig = mock_get_callback("ifconfig");

    inet_aton("1.2.3.5", &ip_address);
    mock_push_ml_network_interface_ip_address("eth1", &ip_address, 0);
    mac_address[0] = 5;
    mac_address[1] = 6;
    mac_address[2] = 7;
    mac_address[3] = 8;
    mac_address[4] = 9;
    mac_address[5] = 10;
    mock_push_ml_network_interface_mac_address("eth1", &mac_address[0], 0);
    mock_push_ml_network_interface_index("eth1", 5, 0);

    CAPTURE_OUTPUT(output, errput) {
        ASSERT_EQ(command_ifconfig(membersof(argv), argv), 0);
    }

    ASSERT_EQ(output,
              "IP Address:  1.2.3.5\n"
              "MAC Address: 05:06:07:08:09:0a\n"
              "Index:       5\n");
}

TEST(command_ifconfig_print_ip_failure)
{
    ml_shell_command_callback_t command_ifconfig;
    const char *argv[] = { "ifconfig", "eth1" };
    struct in_addr ip_address;
    uint8_t mac_address[6];

    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    command_ifconfig = mock_get_callback("ifconfig");

    inet_aton("1.2.3.5", &ip_address);
    mock_push_ml_network_interface_ip_address("eth1", &ip_address, -1);
    mac_address[0] = 5;
    mac_address[1] = 6;
    mac_address[2] = 7;
    mac_address[3] = 8;
    mac_address[4] = 9;
    mac_address[5] = 10;
    mock_push_ml_network_interface_mac_address("eth1", &mac_address[0], 0);
    mock_push_ml_network_interface_index("eth1", 5, 0);

    CAPTURE_OUTPUT(output, errput) {
        ASSERT_EQ(command_ifconfig(membersof(argv), argv), 0);
    }

    ASSERT_EQ(output,
              "IP Address:  failure\n"
              "MAC Address: 05:06:07:08:09:0a\n"
              "Index:       5\n");
}

TEST(command_udp_send_no_args)
{
    ml_shell_command_callback_t command_udp_send;
    const char *argv[] = { "udp_send" };

    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    command_udp_send = mock_get_callback("udp_send");

    CAPTURE_OUTPUT(output, errput) {
        ASSERT_EQ(command_udp_send(membersof(argv), argv), -1);
    }

    ASSERT_EQ(output, "udp_send <ip-address> <port> <data>\n");
}

TEST(command_udp_send_bad_ip_address)
{
    ml_shell_command_callback_t command_udp_send;
    const char *argv[] = { "udp_send", "b.b.c.d", "9999", "Hello" };

    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    command_udp_send = mock_get_callback("udp_send");

    CAPTURE_OUTPUT(output, errput) {
        ASSERT_EQ(command_udp_send(membersof(argv), argv), -1);
    }

    ASSERT_SUBSTRING(output, "udp_send <ip-address> <port> <data>\n");
}

TEST(command_udp_send_open_socket_failure)
{
    ml_shell_command_callback_t command_udp_send;
    const char *argv[] = { "udp_send", "1.2.3.4", "9999", "Hello" };

    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    command_udp_send = mock_get_callback("udp_send");

    CAPTURE_OUTPUT(output, errput) {
        mock_push_socket(AF_INET, SOCK_DGRAM, 0, -1);
        ASSERT_EQ(command_udp_send(membersof(argv), argv), -1);
    }

    ASSERT_SUBSTRING(errput, "error: socket:");
    ASSERT_SUBSTRING(output, "udp_send <ip-address> <port> <data>\n");
}

TEST(command_udp_send_sendto_failure)
{
    int fd;
    ml_shell_command_callback_t command_udp_send;
    const char *argv[] = { "udp_send", "1.2.3.4", "1234", "Hello!" };
    struct sockaddr_in other;

    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    command_udp_send = mock_get_callback("udp_send");
    fd = 9;

    CAPTURE_OUTPUT(output, errput) {
        mock_push_socket(AF_INET, SOCK_DGRAM, 0, fd);
        memset(&other, 0, sizeof(other));
        other.sin_family = AF_INET;
        other.sin_port = htons(1234);
        inet_aton("1.2.3.4", &other.sin_addr);
        mock_push_sendto(fd,
                         "Hello!",
                         6,
                         (struct sockaddr *)&other,
                         sizeof(other),
                         -1);
        mock_push_ml_close(fd, 0);
        ASSERT_EQ(command_udp_send(membersof(argv), argv), -1);
    }

    ASSERT_SUBSTRING(errput, "sendto failed:");
    ASSERT_SUBSTRING(output, "udp_send <ip-address> <port> <data>\n");
}

TEST(command_udp_send)
{
    int fd;
    ml_shell_command_callback_t command_udp_send;
    const char *argv[] = { "udp_send", "1.2.3.4", "1234", "Hello!" };
    struct sockaddr_in other;

    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    command_udp_send = mock_get_callback("udp_send");
    fd = 9;

    CAPTURE_OUTPUT(output, errput) {
        mock_push_socket(AF_INET, SOCK_DGRAM, 0, fd);
        memset(&other, 0, sizeof(other));
        other.sin_family = AF_INET;
        other.sin_port = htons(1234);
        inet_aton("1.2.3.4", &other.sin_addr);
        mock_push_sendto(fd,
                         "Hello!",
                         6,
                         (struct sockaddr *)&other,
                         sizeof(other),
                         6);
        mock_push_ml_close(fd, 0);
        ASSERT_EQ(command_udp_send(membersof(argv), argv), 0);
    }

    ASSERT_EQ(output, "");
}

TEST(command_udp_recv_no_args)
{
    ml_shell_command_callback_t command_udp_recv;
    const char *argv[] = { "udp_recv" };

    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    command_udp_recv = mock_get_callback("udp_recv");

    CAPTURE_OUTPUT(output, errput) {
        ASSERT_EQ(command_udp_recv(membersof(argv), argv), -1);
    }

    ASSERT_EQ(output, "udp_recv <port> [<timeout in seconds>]\n");
}

TEST(command_udp_recv_open_socket_failure)
{
    ml_shell_command_callback_t command_udp_recv;
    const char *argv[] = { "udp_recv", "9999" };

    ml_shell_init();

    mock_push_ml_network_init();
    ml_network_init();

    command_udp_recv = mock_get_callback("udp_recv");

    CAPTURE_OUTPUT(output, errput) {
        mock_push_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, -1);
        ASSERT_EQ(command_udp_recv(membersof(argv), argv), -1);
    }

    ASSERT_SUBSTRING(errput, "error: socket:");
    ASSERT_SUBSTRING(output, "udp_recv <port> [<timeout in seconds>]\n");
}
