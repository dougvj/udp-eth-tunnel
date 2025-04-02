#pragma once
#include "ip_addr.h"
#include "util.h"

typedef struct {
  int num_queues;                   // If 0 then use the default of nprocessors
  const char* default_remote_host;  // This is the default upstream remote
                                    // host to forward frames to. If not
  int default_remote_port;  // specified, then the tunnel will not forward
                            // frames to a remote host
  const char*
      allowed_remote_hosts;  // This is a list of CIDR addresses that are
                             // allowed to be used as remote hosts. Each
                             // new unique packet from an allowed host
                             // on the port will create a new TAP interface
                             // for that host. If not specified, then only
                             // the default remote host will be allowed to
                             // send packets
  const char* bind_iface;    // The interface to bind to. This is the interface
                             // that the tunnel will listen on for incoming
                             // packets If NULL, then the tunnel will listen on
                           // all interfaces. A warning will be printed if this
                           // is not set, because it's extremely dangerous
                           // to listen on all interfaces without proper
                           // allowed_remote_hosts
  int bind_port;  // The port to bind to. This is the port that the tunnel will
                  // listen on for incoming packets. If 0, then the
                  // tunnel will automatically select a port. This
                  // is only useful when a default remote host is specified
  int mtu;        // This is the mtu of the tap device. If not specified,
                  // the default is the mtu of the bind interface - overhead
                  // Note that there is no support for internal packet
                  // fragmentation, but you can maybe get away with
                  // using a larger mtu if you know that your udp
            // packets will fragement (this appears to be the case with ipv4
            // wireguard) however performance will suffer. This may be
            // acceptable for maximum compatibility with all ethernet devices
  const char* tunnel_name;    // The name of the tunnel. Required for logging,
                              // but the config parser will set this to a
                              // the value 'default' if not specified
  const char* tap_base_name;  // The base name of the tap devices for this
                              // tunnel. If null, this is just 'tap' and the
                              // first available numeric suffix will be used for
                              // each newly allocated TAP device
  const char*
      bridge_name;  // The name of the bridge to add the tap devices to
                    // If null, a bridge based on the tunnel name will be
                    // used. If the bridge does not exist, it will be created
} tunnel_params;

typedef struct tunnel tunnel;

tunnel* tunnel_create(tunnel_params* params);
void tunnel_wait(tunnel* tunnel);
void tunnel_print_stats(tunnel* tunnel);
void tunnel_delete(tunnel* tunnel);

// This is a simple list of tunnels
typedef struct tunnel_list tunnel_list;
tunnel_list* tunnel_list_create();
void tunnel_list_delete(tunnel_list* list);
size_t tunnel_list_size(tunnel_list* list);
bool tunnel_list_add(tunnel_list* list, tunnel* tunnel);
// TODO remove?
tunnel* tunnel_list_get(tunnel_list* list, int index);
