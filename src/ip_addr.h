#pragma once
#include "util.h"
#include <arpa/inet.h>

#define MAX_IP_ADDR_STR_LEN INET6_ADDRSTRLEN

typedef struct ip_addr ip_addr;
typedef struct ip_addr_cidr ip_addr_cidr;
typedef struct ip_addr_cidr_list ip_addr_cidr_list;

// This is always in network byte order and masks can still work
struct ip_addr {
  union {
    u128 addr;
    struct {
      u64 high_qword;
      u64 low_qword;
    };
    struct {
      u32 dwords[4];
    };
    struct in6_addr sys_addr;
  };
};


bool ip_addr_from_str(const char* str, ip_addr* addr);
bool ip_addr_is_ipv4(ip_addr* addr);
bool ip_addr_is_str(const char* str);
bool ip_addr_to_str(ip_addr* addr, char* str);

// This is a CIDR address, which is an IP address and a prefix length
// However we store it as a mask and an address for simpler comparison
// and matching
struct ip_addr_cidr {
  ip_addr mask;
  ip_addr addr;
};

bool ip_addr_cidr_from_str(const char* str, ip_addr_cidr* addr_cidr);
bool ip_addr_cidr_match(ip_addr_cidr* addr_cidr, ip_addr* addr);

// This is a list of CIDR addresses, which is used for matching of allowed
// addresses
struct ip_addr_cidr_list {
  size_t num_entries;
  ip_addr_cidr entries[];
};

ip_addr_cidr_list* ip_addr_cidr_list_create_from_str(const char* str);
bool ip_addr_cidr_list_match(ip_addr_cidr_list* list,
                             ip_addr* addr);
void ip_addr_cidr_list_delete(ip_addr_cidr_list* list);
