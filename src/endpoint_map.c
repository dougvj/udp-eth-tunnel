#include "endpoint_map.h"
#include "ip_addr.h"
#include "stdlib.h"

typedef struct {
  ip_addr remote_addr;  // The remote address
  u16 port;             // The remote port
  bool present : 1;     // True if this entry is present
  int tap_id : 31;      // The id of the tap device
} endpoint_map_entry;

static_assert(sizeof(endpoint_map_entry) == sizeof(u64) * 4,
              "endpoint_map_entry must be nicely sized");

struct endpoint_map {
  size_t num_entries;
  size_t alloc_entries;
  u32 nonce_a;  // For cuckoo hash we have two hashes
  u32 nonce_b;
  endpoint_map_entry entries[];
};

endpoint_map* endpoint_map_create() {
  int alloc_entries = 32;
  endpoint_map* map = calloc(
      1, sizeof(endpoint_map) + alloc_entries * sizeof(endpoint_map_entry));
  if (!map) {
    perror("endpoint_map_create: malloc");
    return NULL;
  }
  map->alloc_entries = alloc_entries;
  map->num_entries = 0;
  map->nonce_a = rand();
  map->nonce_b = rand();
  return map;
}

static u32 hash(ip_addr* addr, u16 port, u32 nonce) {
  u32 hash = nonce;
  for (size_t i = 0; i < sizeof(ip_addr) / 4; i++) {
    hash ^= ((u32*)addr)[i];
  }
  hash ^= port;
  return hash;
}

static u8 hash_small(ip_addr* addr, u16 port, u32 nonce) {
  u8* nonce_bytes = (u8*)&nonce;
  u8 hash = nonce_bytes[0] ^ nonce_bytes[1] ^ nonce_bytes[2] ^ nonce_bytes[3];
  for (size_t i = 0; i < sizeof(ip_addr) / 4; i++) {
    hash ^= ((u8*)addr)[i];
  }
  u8* port_bytes = (u8*)&port;
  hash ^= port_bytes[0] ^ port_bytes[1];
  return hash;
}

static bool try_insert(endpoint_map* map,
                        ip_addr* addr,
                        u16 port,
                        int tap_id) {
  u32 hash_a, hash_b;
  // Alloc entries is always a power of 2
  if (map->alloc_entries <= 256) {
    hash_a = hash_small(addr, port, map->nonce_a) & (map->alloc_entries - 1);
    hash_b = hash_small(addr, port, map->nonce_b) & (map->alloc_entries - 1);
  } else {
    hash_a = hash(addr, port, map->nonce_a) & (map->alloc_entries - 1);
    hash_b = hash(addr, port, map->nonce_b) & (map->alloc_entries - 1);
  }
  endpoint_map_entry* entry_a = &map->entries[hash_a];
  endpoint_map_entry* entry_b = &map->entries[hash_b];
  endpoint_map_entry* entries[] = {entry_a, entry_b};
  for (int i = 0; i < 2; i++) {
    endpoint_map_entry* entry = entries[i];
    if (entry->present) {
      if (addr->addr == entry->remote_addr.addr && entry->port == port) {
        entry->tap_id = tap_id;
        return true;
      }
    } else {
      entry->remote_addr = *addr;
      entry->port = port;
      entry->tap_id = tap_id;
      entry->present = true;
      map->num_entries++;
      return true;
    }
  }
  return false;
}

endpoint_map* endpoint_map_add(endpoint_map* map,
                            ip_addr* addr,
                            u16 port,
                            int tap_id) {
  if (map->num_entries >= map->alloc_entries) {
    // We need to expand the table
    int new_size = map->alloc_entries * 2;
    endpoint_map* new_map =
        realloc(map, sizeof(endpoint_map) + new_size * sizeof(endpoint_map_entry));
    if (!new_map) {
      perror("endpoint_map_add: realloc");
      return NULL;
    }
    map = new_map;
  } else {
    if (try_insert(map, addr, port, tap_id)) {
      return map;
    }
  }
  // Try to regenerate the entire table because we either had to reallocate
  // or we failed to insert the entry
  endpoint_map_entry* table =
      malloc(sizeof(endpoint_map_entry) * map->alloc_entries);
  if (!table) {
    perror("endpoint_map_add: malloc");
    return NULL;
  }
  // Copy the entries because we are gonna iterate over them
  memcpy(table, map->entries,
         sizeof(endpoint_map_entry) * map->alloc_entries);
  int retries = 0;
  for(;;) {
    // Reset the target table
    memset(map->entries, 0,
          sizeof(endpoint_map_entry) * map->alloc_entries);
    map->num_entries = 0;
    for (size_t i = 0; i < map->num_entries; i++) {
      if (!try_insert(map, &table[i].remote_addr, table[i].port,
                    table[i].tap_id)) {
        // We failed to insert the entry, we need to rehash with new
        // nonces
        map->nonce_a = rand();
        map->nonce_b = rand();
        retries++;
        break;
      }
    }
  }
  if (retries > 0) {
    LOG("endpoint_map_add: found perfect cuckoo hash after %d retries",
        retries);
  }
  free(table);
  return map;
}

int endpoint_map_lookup(endpoint_map* map, ip_addr* addr, u16 port) {
  u32 hash_a, hash_b;
  if (map->alloc_entries <= 256) {
    hash_a = hash_small(addr, port, map->nonce_a) & (map->alloc_entries - 1);
    hash_b = hash_small(addr, port, map->nonce_b) & (map->alloc_entries - 1);
  } else {
    hash_a = hash(addr, port, map->nonce_a) & (map->alloc_entries - 1);
    hash_b = hash(addr, port, map->nonce_b) & (map->alloc_entries - 1);
  }
  endpoint_map_entry* entry_a = &map->entries[hash_a];
  endpoint_map_entry* entry_b = &map->entries[hash_b];
  endpoint_map_entry* entries[] = {entry_a, entry_b};
  for (int i = 0; i < 2; i++) {
    endpoint_map_entry* entry = entries[i];
    if (entry->present) {
      if (addr->addr == entry->remote_addr.addr && entry->port == port) {
        return entry->tap_id;
      }
    }
  }
  return -1;
}

bool endpoint_map_remove(endpoint_map* map, ip_addr* addr, u16 port) {
  u32 hash_a, hash_b;
  if (map->alloc_entries <= 256) {
    hash_a = hash_small(addr, port, map->nonce_a) & (map->alloc_entries - 1);
    hash_b = hash_small(addr, port, map->nonce_b) & (map->alloc_entries - 1);
  } else {
    hash_a = hash(addr, port, map->nonce_a) & (map->alloc_entries - 1);
    hash_b = hash(addr, port, map->nonce_b) & (map->alloc_entries - 1);
  }
  endpoint_map_entry* entry_a = &map->entries[hash_a];
  endpoint_map_entry* entry_b = &map->entries[hash_b];
  endpoint_map_entry* entries[] = {entry_a, entry_b};
  for (int i = 0; i < 2; i++) {
    endpoint_map_entry* entry = entries[i];
    if (entry->present) {
      if (addr->addr == entry->remote_addr.addr && entry->port == port) {
        entry->present = false;
        map->num_entries--;
        return true;
      }
    }
  }
  return false;
}

void endpoint_map_delete(endpoint_map* map) {
  free(map);
}
