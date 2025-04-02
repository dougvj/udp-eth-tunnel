#pragma once
#include "ip_addr.h"
#include "util.h"

typedef struct endpoint_map endpoint_map;

endpoint_map* endpoint_map_create();
void endpoint_map_delete(endpoint_map* map);
endpoint_map* endpoint_map_add(endpoint_map* map,
                             ip_addr* addr,
                             u16 port,
                             int tap_id);
int endpoint_map_lookup(endpoint_map* map, ip_addr* addr, u16 port);
bool endpoint_map_remove(endpoint_map* map, ip_addr* addr, u16 port);
