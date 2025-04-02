#pragma once
#include "ip_addr.h"
#include "util.h"

bool net_create_tap_mq_fds(const char* base_tap_dev,
                           int num_queues,
                           int* fds,
                           int mtu,
                           const char* bridge,
                           char* tap_dev_resolved);

bool net_create_udp_sock_fds(const char* bind_iface,
                             int bind_port,
                             int num_queues,
                             int* fds,
                             int* if_mtu);

bool net_set_nonblocking(int fd);

bool net_lookup_remote_host(const char* host, ip_addr* out_addr);
