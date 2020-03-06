/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020, Erik Moqvist
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
#include "ml/ml.h"
#include <linux/netfilter/xt_conntrack.h>

struct match_tcp_t {
    struct xt_entry_match match;
    struct xt_tcp tcp;
};

struct match_conntrack_t {
    struct xt_entry_match match;
    struct xt_conntrack_mtinfo3 mtinfo3;
};

struct http_entry_t {
    struct ipt_entry entry;
    struct match_tcp_t tcp;
    struct match_conntrack_t conntrack;
    struct xt_standard_target target;
};

struct standard_entry_t {
    struct ipt_entry entry;
    struct xt_standard_target target;
};

struct error_entry_t {
    struct ipt_entry entry;
    struct xt_error_target target;
};

struct replace_t {
    struct ipt_replace header;
    struct standard_entry_t input;
    struct standard_entry_t forward;
    struct http_entry_t http;
    struct standard_entry_t output;
    struct error_entry_t error;
};

/* iptables -A OUTPUT -p tcp --dport 80 -m state --state NEW -j
   DROP */
static void drop_http()
{
    struct replace_t replace;
    struct standard_entry_t *standard_p;
    struct error_entry_t *error_p;
    struct http_entry_t *http_p;
    struct ipt_replace *header_p;
    struct ipt_get_entries *entries_p;
    struct ipt_getinfo info;

    entries_p = ml_network_filter_ipv4_get("filter", &info);

    if (entries_p == NULL) {
        ml_warning("Failed to read entries.");

        return;
    }

    memset(&replace, 0, sizeof(replace));

    /* Header. */
    header_p = &replace.header;
    strcpy(&header_p->name[0], "filter");
    header_p->valid_hooks = ((1 << NF_IP_LOCAL_IN)
                             | (1 << NF_IP_FORWARD)
                             | (1 << NF_IP_LOCAL_OUT));
    header_p->num_entries = 5;
    header_p->size = sizeof(replace) - sizeof(replace.header);
    header_p->hook_entry[NF_INET_LOCAL_IN] = 0;
    header_p->hook_entry[NF_INET_FORWARD] = sizeof(replace.input);
    header_p->hook_entry[NF_INET_LOCAL_OUT] = (sizeof(replace.input)
                                               + sizeof(replace.forward));
    header_p->underflow[NF_INET_LOCAL_IN] = 0;
    header_p->underflow[NF_INET_FORWARD] = sizeof(replace.input);
    header_p->underflow[NF_INET_LOCAL_OUT] = (sizeof(replace.input)
                                              + sizeof(replace.forward)
                                              + sizeof(replace.http));
    header_p->num_counters = info.num_entries;

    /* Input. */
    standard_p = &replace.input;
    standard_p->entry.target_offset = offsetof(struct standard_entry_t, target);
    standard_p->entry.next_offset = sizeof(*standard_p);
    standard_p->target.target.u.target_size = sizeof(standard_p->target);
    standard_p->target.verdict = -NF_ACCEPT - 1;

    /* Forward. */
    standard_p = &replace.forward;
    standard_p->entry.target_offset = offsetof(struct standard_entry_t, target);
    standard_p->entry.next_offset = sizeof(*standard_p);
    standard_p->target.target.u.target_size = sizeof(standard_p->target);
    standard_p->target.verdict = -NF_ACCEPT - 1;

    /* Http output. */
    http_p = &replace.http;
    http_p->entry.target_offset = offsetof(struct http_entry_t, target);
    http_p->entry.next_offset = sizeof(*http_p);
    http_p->entry.ip.proto = IPPROTO_TCP;
    http_p->tcp.match.u.user.match_size = sizeof(http_p->tcp);
    strcpy(&http_p->tcp.match.u.user.name[0], "tcp");
    http_p->tcp.tcp.spts[0] = 0;
    http_p->tcp.tcp.spts[1] = 65535;
    http_p->tcp.tcp.dpts[0] = 80;
    http_p->tcp.tcp.dpts[1] = 80;
    http_p->conntrack.match.u.user.match_size = sizeof(http_p->conntrack);
    strcpy(&http_p->conntrack.match.u.user.name[0], "conntrack");
    http_p->conntrack.match.u.user.revision = 3;
    http_p->conntrack.mtinfo3.match_flags = (XT_CONNTRACK_STATE
                                             | XT_CONNTRACK_STATE_ALIAS);
    http_p->conntrack.mtinfo3.state_mask = NF_CT_STATE_BIT(IP_CT_NEW);
    http_p->target.target.u.target_size = sizeof(http_p->target);
    http_p->target.verdict = -NF_DROP - 1;

    /* Output. */
    standard_p = &replace.output;
    standard_p->entry.target_offset = offsetof(struct standard_entry_t, target);
    standard_p->entry.next_offset = sizeof(*standard_p);
    standard_p->target.target.u.target_size = sizeof(standard_p->target);
    standard_p->target.verdict = -NF_ACCEPT - 1;

    /* Error. */
    error_p = &replace.error;
    error_p->entry.target_offset = offsetof(struct error_entry_t, target);
    error_p->entry.next_offset = sizeof(*error_p);
    error_p->target.target.u.user.target_size = sizeof(error_p->target);
    strcpy(&error_p->target.target.u.user.name[0], "ERROR");
    strcpy(&error_p->target.errorname[0], "ERROR");

    ml_network_filter_ipv4_set(&replace.header);
}

int main()
{
    ml_init();

    ml_network_filter_ipv4_log("filter");
    ml_info("Drop all.");
    ml_network_filter_ipv4_drop_all();
    ml_network_filter_ipv4_log("filter");
    ml_info("Drop http.");
    drop_http();
    ml_network_filter_ipv4_log("filter");
    ml_info("Accept all.");
    ml_network_filter_ipv4_accept_all();
    ml_network_filter_ipv4_log("filter");

    return (0);
}
