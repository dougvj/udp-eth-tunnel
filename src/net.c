#include "net.h"
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/sockios.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "ip_addr.h"

// TODO we should return the tap name used
bool net_create_tap_mq_fds(
                  const char* base_tap_dev,
                  int queues,
                  int* fds,
                  int mtu,
                  const char* bridge,
                  char* tap_dev_resolved) {
  char tap_name[IFNAMSIZ];
  const char* dev = base_tap_dev;
  if (!dev) {
    dev = "tap";
  }
  int highest_tap_num = -1;
  FILE* fp = fopen("/proc/net/dev", "r");
  if (fp) {
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
      if (strstr(line, dev) != NULL) {
        char* name = strstr(line, dev);
        if (name) {
          name += strlen(dev);
          int tap_num = atoi(name);
          if (tap_num > highest_tap_num) {
            highest_tap_num = tap_num;
          }
        }
      }
    }
    fclose(fp);
  }
  memset(fds, 0, queues * sizeof(int));
  int len = snprintf(tap_name, IFNAMSIZ, "%s%d", dev, highest_tap_num + 1);
  if (len < 0 || len >= IFNAMSIZ) {
    LOG("TAP device name '%s' is too long", dev);
    return false;
  }
  struct ifreq ifr = {
      .ifr_flags = IFF_TAP | IFF_NO_PI | IFF_MULTI_QUEUE,
  };
  if (strlen(tap_name) >= IFNAMSIZ) {
    LOG("TAP device name '%s' is too long", tap_name);
    return false;
  }
  LOG_DEBUG("Creating TAP device %s", tap_name);
  strlcpy(ifr.ifr_name, tap_name, IFNAMSIZ);
  if (tap_dev_resolved) {
    strlcpy(tap_dev_resolved, tap_name, IFNAMSIZ);
  }
  int i;
  for (i = 0; i < queues; i++) {
    int tun_fd = PERROR_IF_NEG(open("/dev/net/tun", O_RDWR));
    if (tun_fd < 0) {
      goto error;
    }
    if (PERROR_IF_NEG(ioctl(tun_fd, TUNSETIFF, &ifr)) < 0) {
      LOG("Failed to create tap device '%s'", tap_name);
      close(tun_fd);
      goto error;
    }
    fds[i] = tun_fd;
  }
  {  // Block for ctl_fd lifetime
    int ctl_fd defer(close) = PERROR_IF_NEG(socket(AF_INET, SOCK_DGRAM, 0));
    if (ctl_fd < 0) {
      goto error;
    }
    // Get the TAP device
    memset(&ifr, 0, sizeof(ifr));
    strlcpy(ifr.ifr_name, tap_name, IFNAMSIZ - 1);
    ioctl_or_goto(error, ctl_fd, SIOCGIFINDEX, &ifr);
    LOG_DEBUG("TAP device %s index %d", tap_name, ifr.ifr_ifindex);
    int tap_index = ifr.ifr_ifindex;
    if (mtu > 0) {
      // Set the MTU for the TAP device
      ifr.ifr_mtu = mtu;
      if (PERROR_IF_NEG(ioctl(ctl_fd, SIOCSIFMTU, &ifr) < 0)) {
        goto error;
      }
      LOG_DEBUG("TAP device %s MTU set to %d", tap_name, mtu);
    }
    if (bridge != NULL) {
      // Query the bridge interface
      struct ifreq br_ifr;
      memset(&br_ifr, 0, sizeof(br_ifr));
      strncpy(br_ifr.ifr_name, bridge, IFNAMSIZ - 1);
      if (PERROR_IF_NEG(ioctl(ctl_fd, SIOCGIFINDEX, &br_ifr)) < 0) {
        if (errno != ENODEV) {
          goto error;
        }
        LOG("Bridge interface '%s' not found, creating it", bridge);
        // Create the bridge Interface
        ioctl_or_goto(error, ctl_fd, SIOCBRADDBR, &br_ifr);
        // Set the bridge interface up
        ioctl_or_goto(error, ctl_fd, SIOCGIFFLAGS, &br_ifr);
        br_ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
        ioctl_or_goto(error, ctl_fd, SIOCSIFFLAGS, &br_ifr);
      }
      // Query the bridge's MTU and warn if it is different
      if (PERROR_IF_NEG(ioctl(ctl_fd, SIOCGIFMTU, &br_ifr) < 0)) {
        goto error;
      }
      int br_mtu = br_ifr.ifr_mtu;
      if (mtu > 0 && mtu != br_mtu) {
        LOG("Warning: TAP device MTU %d is different from bridge MTU %d",
            mtu, br_mtu);
      }
      // Add the TAP device to the bridge
      br_ifr.ifr_ifindex = tap_index;
      ioctl_or_goto(error, ctl_fd, SIOCBRADDIF, &br_ifr);
    } else {
      LOG_DEBUG("No bridge specified, not adding TAP device to a bridge");
    }
    // Set the TAP device up
    ioctl_or_goto(error, ctl_fd, SIOCGIFFLAGS, &ifr);
    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
    ioctl_or_goto(error, ctl_fd, SIOCSIFFLAGS, &ifr);
  }
  return true;
error:
  for (int j = 0; j < i; j++) {
    if (fds[j] > 0) {
      close(fds[j]);
    }
  }
  return false;
}

 bool net_lookup_remote_host(const char* host, ip_addr* out_addr) {
  struct addrinfo* ai;
  if (PERROR_IF_NEG(getaddrinfo(host, NULL, NULL, &ai)) < 0) {
    return false;
  }
  bool success = false;
  struct addrinfo* ai_next = ai;
  while (ai_next) {
    char ip[INET6_ADDRSTRLEN];
    int af = ai_next->ai_family;
    if (inet_ntop(af, &((struct sockaddr_in*)ai_next->ai_addr)->sin_addr, ip,
                  sizeof(ip)) == NULL) {
      perror("alloc_udp_socket_client: inet_ntop");
      ai_next = ai_next->ai_next;
    }
    LOG_DEBUG("Resolved %s to %s", host, ip);
    if (af == AF_INET) {
      // Convert to IPv6 mapped address
      memset(&out_addr->sys_addr, 0, sizeof(out_addr->sys_addr));
      out_addr->sys_addr.s6_addr[10] = 0xff;
      out_addr->sys_addr.s6_addr[11] = 0xff;
      memcpy(&out_addr->sys_addr.s6_addr[12],
             &((struct sockaddr_in*)ai_next->ai_addr)->sin_addr,
             sizeof(struct in_addr));
    } else if (af == AF_INET6) {
      out_addr->sys_addr = ((struct sockaddr_in6*)ai_next->ai_addr)->sin6_addr;
    }
    success = true;
    goto end;
  }
end:
  freeaddrinfo(ai);
  return success;
}

// TODO specify a flag for preferring IPv4 or IPv6
bool net_create_udp_sock_fds(const char* bind_iface,
                       int bind_port,
                       int num_queues,
                       int* fds,
                       int* if_mtu) {
  struct sockaddr_in6 local_addr;
  // Get the local address of the given interface
  memset(&local_addr, 0, sizeof(local_addr));
  local_addr.sin6_family = AF_INET6;
  local_addr.sin6_port = htons(bind_port);
  // TODO query /proc/net/if_net6
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, bind_iface, IFNAMSIZ - 1);
  int defer(close) ctl_fd = PERROR_IF_NEG(socket(AF_INET, SOCK_DGRAM, 0));
  if (PERROR_IF_NEG(ioctl(ctl_fd, SIOCGIFADDR, &ifr)) < 0) {
    if (errno == ENODEV) {
      LOG("Interface %s not found", bind_iface);
    }
    return false;
  }
  if (ifr.ifr_addr.sa_family == AF_INET) {
    // Convert to IPv6 mapped address
    memset(&local_addr.sin6_addr, 0, sizeof(local_addr.sin6_addr));
    local_addr.sin6_addr.s6_addr[10] = 0xff;
    local_addr.sin6_addr.s6_addr[11] = 0xff;
    memcpy(&local_addr.sin6_addr.s6_addr[12],
           &((struct sockaddr_in6*)&ifr.ifr_addr)->sin6_addr,
           sizeof(struct in_addr));
  } else if (ifr.ifr_addr.sa_family == AF_INET6) {
    LOG("IOCTL returned IPv6 address, unexpected");
    return false;
  }
  if (if_mtu) {
    //local_addr.sin_family = ((struct sockaddr_in*)&ifr.ifr_addr)->sin_family;
    if (PERROR_IF_NEG(ioctl(ctl_fd, SIOCGIFMTU, &ifr)) < 0) {
      return false;
    }
    int bind_mtu = ifr.ifr_mtu;
    LOG_DEBUG("Bind MTU: %d", bind_mtu);
    *if_mtu = bind_mtu;
  }
  for (int i = 0; i < num_queues; i++) {
    int fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (fd < 0) {
      perror("alloc_udp_socket_client: socket");
      close(fd);
      return false;
    }
    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
      perror("alloc_udp_socket_client: setsockopt SO_REUSEPORT");
      close(fd);
      return false;
    }
    if (bind(fd, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
      perror("alloc_udp_socket_client: bind");
      close(fd);
      return false;
    }
    fds[i] = fd;
  }
  return true;
}


bool net_set_nonblocking(int fd) {
  int flags = PERROR_IF_NEG(fcntl(fd, F_GETFL, 0));
  if (flags < 0) {
    perror("set_nonblocking: fcntl F_GETFL");
    return false;
  }
  if (PERROR_IF_NEG(fcntl(fd, F_SETFL, flags | O_NONBLOCK)) < 0) {
    perror("set_nonblocking: fcntl F_SETFL");
    return false;
  }
  return true;
}
