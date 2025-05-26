# UDP-eth-tunnel

Tunnel Layer 2 Ethernet frames over Layer 3 UDP.

## Description

This is a linux-specific utility that manages tunneling ethernet frames over UDP
between several endpoints. It doesn't provide any security with the exception
of IP allow lists, so it is recommended to use it over a secure tunnel such as
wireguard.

The utility is similar in concept to OpenVPN's TAP mode with crypto disabled,
but with the notable difference that instead of a single TAP interface per
tunnel, it creates a TAP interface for each endpoint and adds them to a bridge, 
letting the kernel handle packet switching.

## Features

- Simple to use. One configuration file, one command line argument.
- Support several tunnels defined in a single configuration file.
- Multithreaded receive and transmit queues for maximum throughput using
  Linux's epoll interface
- Automatically create a bridge for the tunnel or automatically add tunneled
  ethernet interfaces to an existing bridge. Packets received on the tunnel
  are routed to a unique TAP interface based on the source IP and port
- Peer to peer communication, no client/server model. Default endpoints allow
  for forwarding all unknown packets in a kind of virtual "uplink"
- Support explicit endpoint configuration or automatic endpoint
  configuration based on allowed IPs/subnets or both
- No dependencies other than Linux and libc with strlcat (libbsd/newer glibc)
- SIGUSR1 shows tunnel statistics

## Pitfalls

- You absolutely must ensure security via another layer such as wireguard
  or IPsec. This utility does not provide any security beyond IP allow lists.
  Be very careful that you either don't care about security or you trust the
  layer 3 network you're tunneling over
- No support for protocol level fragmentation. Layer 2 Ethernet MTUs are
  generally 1500 bytes, which means that tunneling 1500 byte frames over UDP
  requires fragmentation support most of the time. If you are careful,
  you can manually set the MTU to a lower value in the config file, but this
  will probably result in dropped packets. The default MTU is set to
  1500. Fortunately, wireguard and other layer 3 tunnels do support
  fragmentation (at least over IPv4), so 1500 bytes works for this case.
- This likely breaks over NAT because there is no keep alive or similar
  mechanism. Fortunately, wireguard again solves this problem.

## Wishlist
- Internal fragmentation support.
- Support configuring wireguard tunnels automatically with the same config file.
  (I looked into configuring wireguard tunnels and it looks like a pain without
   simply invoking the wg binary)
- Install systemd service file

## Installation
```bash
meson build
sudo ninja -C build install
```

## Usage

```bash
udp-eth-tunnel /path/to/config/file
```

## Configuration

```ini
; UDP eth-tunnel configuration file
[section] ; The section name is the tunnel name in the logs.
          ; the section name can be omitted for a single 'default' tunnel
; These are required options and must be set.
bind_iface = wg0 ; The interface to bind to. This is required. The rationale
                 ; for this requirement is to avoid security issues by
                 ; accidentally binding to the wrong interface or listening on
                 ; all interfaces.
bind_port = 12345 ; The UDP port to bind to.
default_endpoint = 127.0.0.1:1234 ; The default endpoint for the tunnel. This
                                  ; is required if allwed_remote_hosts is not
                                  ; set. The default endpoint is used to set
                                  ; a default TAP interface for the tunnel.
allowed_remote_hosts = 10.0.0.0/8,192.168.0.0/16 ; A comma separated list of
                          ; allowed remote hosts. This is required if the
                          ; default endpoint is not set. This may be a single
                          ; IP address, a CIDR network, or a comma separated
                          ; list of either.
; The following options are optional and can be omitted.
bridge_name = br0 ; The bridge name to add the tap interface to. If not specified,
                  ; the bridge name will be br-<section-name>. Recall that no
                  ; section name means 'default' so you will get 'br-default'.
                  ; If the bridge does not exist, it will be created. You
                  ; may specify an already configured bridge name.
mtu = 1500 ; The MTU of the tap interface, optional. By default will be set to
             1500
num_queues = 4 ; The number of queues to create for the tap interface, optional.
             ; By default will be set to the number of CPU cores detected

tap_base_name = tap ; The base name for the tap interfaces. The default is 'tap'. This is
                  ; used to create the tap interface names. The default will
                  ; create tap0, tap1, etc. If you set this to 'foo', the tap
                  ; interfaces will be foo0, foo1, etc. The number postfixed is
                  ; automatically incremeneted from available free interface
                  ; names, so multiple tunnels can use the same base name or you
                  ; can simply use the default and all endpoint tap interfaces
                  ; will use tapN
```

## Examples

### Example 1: A single host with two bridge interfaces that share layer 2 via this utility, simulating a veth pair

Obviously a real veth pair in kernel is always a better option, this is just
for demonstration purposes.

```ini
[fakeveth1]
bind_iface=lo
bind_port=1234
allowed_remote_hosts=127.0.0.1

[fakeveth0]
bind_iface=lo
bind_port=1235
default_endpoint=127.0.0.1:1234

```

If you set an IP address on the bridge interfaces, you can ping between them.
```bash
ip addr add 10.0.0.1/24 dev fakeveth0
ip addr add 10.0.0.2/24 dev fakeveth1
ping -I fakeveth0 10.0.0.2
ping -I fakeveth1 10.0.0.2
```

### Example 2: A single host acts as a central switch that other hosts connect to

This example creates a single host that acts as a 'ethernet switch' for multiple
remote hosts.  Note that the section names are omitted, so both configurations
adopt the 'default' section name

```ini
; Config on central host
bind_iface=wg0; assume we are 10.10.10.1
bind_port=1234
allowed_remote_hosts=10.10.10.0/24 ; Allow anyone in 10.10.10.x
```

```ini
; Config on remote hosts
bind_iface=wg0 ; assume we have an IP 10.10.10.x and this wirguard tunnel
               ; is connected to the wireguard tunnel on the central host
bind_port=1234
default_endpoint=10.10.10.1:1234
```

For each remote host that connects to the central host, a new tap interface
will be created on the central host. The clients only have a single TAP
interface, but could concievably also have their own allowed_remote_hosts
directive for a complex peer-to-peer setup.

Note that you could enable STP on the bridge and even have a mesh network
between remote hosts.

### Example 3: Listening on multiple interfaces

This example shows how the same layer 2 network can be bridged across multiple
tunnels listening on different interfaces. Because each section defines the
same bridge_name, they are the same layer2 network.

```ini
[interface1]
bind_iface=wg0
bridge_name=tunnelbr0
bind_port=1234
allowed_remote_hosts=10.10.0.0/16

[interface2]
bind_iface=wg1
bridge_name=tunnelbr0
bind_port=1234
allowed_remote_hosts=10.12.0.0/16
```
