#include "ip_addr.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>



bool ip_addr_from_str(const char* str, ip_addr* addr) {
  LOG_DEBUG("ip_addr_from_str: %s", str);
  if (inet_pton(AF_INET6, str, &addr->sys_addr) < 0) {
    LOG("Invalid IP address: %s: %s", str, strerror(errno));
    return false;
  }
  return true;
}

bool ip_addr_cidr_from_str(const char* str,
                            ip_addr_cidr* addr_cidr) {
  char tmp[INET6_ADDRSTRLEN];
  strcpy(tmp, str);
  char* slash = strchr(tmp, '/');
  if (slash) {
    *slash = '\0';
    int prefix_len = atoi(slash + 1);
    if (prefix_len < 0 || prefix_len > 128) {
      LOG("Invalid CIDR prefix length: %s", str);
      return false;
    }
    addr_cidr->mask.addr = ~((u128)0) << (128 - prefix_len);
  } else {
    // No prefix length, so we assume a full mask
    addr_cidr->mask.addr = ~((u128)0);
  }
  // At this point the mask is in LE if we are on a LE platform
  // so we need to convert it to BE
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  addr_cidr->mask.high_qword = htobe64(addr_cidr->mask.high_qword);
  addr_cidr->mask.low_qword = htobe64(addr_cidr->mask.low_qword);
  u64 temp = addr_cidr->mask.high_qword;
  addr_cidr->mask.high_qword = addr_cidr->mask.low_qword;
  addr_cidr->mask.low_qword = temp;
#endif
  if (!ip_addr_from_str(tmp, &addr_cidr->addr)) {
    return false;
  }
  return true;
}

static ip_addr_cidr ipv4_space = {};

void constructor ipv4_space_init() {
  assert(ip_addr_cidr_from_str("0:0:0:0:0:FFFF::/96", &ipv4_space));
}

bool ip_addr_cidr_match(ip_addr_cidr* addr_cidr, ip_addr* addr) {
  if (addr->addr & addr_cidr->mask.addr) {
    return true;
  }
  return false;
}

ip_addr_cidr_list* ip_addr_cidr_list_create_from_str(const char* str) {
  // Count the number of entries (commas + 1)
  int count = 1;
  for (const char* p = str; *p; p++) {
    if (*p == ',') {
      count++;
    }
  }
  ip_addr_cidr_list* list = malloc(sizeof(ip_addr_cidr_list) +
                                    count * sizeof(ip_addr_cidr));
  if (!list) {
    perror("ip_addr_cidr_list_create_from_str: malloc");
    return NULL;
  }
  memset(list, 0, sizeof(ip_addr_cidr_list) + count * sizeof(ip_addr_cidr));
  list->num_entries = count;
  char* str_copy = strdup(str);
  char* token = strtok(str_copy, ",");
  size_t i = 0;
  while (token) {
    if (!ip_addr_cidr_from_str(token, &list->entries[i])) {
      LOG("ip_addr_cidr_list_create_from_str: invalid address: %s", token);
      free(str_copy);
      free(list);
      return NULL;
    }
    token = strtok(NULL, ",");
    i++;
  }
  free(str_copy);
  return list;
}

bool ip_addr_cidr_list_match(ip_addr_cidr_list* list,
                             ip_addr* addr) {
  for (size_t i = 0; i < list->num_entries; i++) {
    if (ip_addr_cidr_match(&list->entries[i], addr)) {
      return true;
    }
  }
  return false;
}



bool ip_addr_is_ipv4(ip_addr* addr) {
  return ip_addr_cidr_match(&ipv4_space, addr);
}

bool ip_addr_to_str(ip_addr* addr, char* str) {
  // Check if we are IPv4. If so we need to convert it to IPv4 sysaddr
  // and then convert it to a string
  if (ip_addr_is_ipv4(addr)) {
    struct in_addr addr_sys = {
        .s_addr = addr->dwords[3] // IPv4 is in the last dword
    };
    if (inet_ntop(AF_INET, &addr_sys, str, MAX_IP_ADDR_STR_LEN) == NULL) {
      perror("ip_addr_to_str: inet_ntop AF_INET");
      return false;
    }
  } else {
    if (inet_ntop(AF_INET6, &addr->sys_addr, str, MAX_IP_ADDR_STR_LEN) == NULL) {
      perror("ip_addr_to_str: inet_ntop AF_INET6");
      return false;
    }
  }
  return true;
}

void ip_addr_cidr_list_delete(ip_addr_cidr_list* list) {
  free(list);
}
