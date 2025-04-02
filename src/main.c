#include "util.h"
#include "tunnel.h"
#include "ini.h"
#include <signal.h>

tunnel_list* __tunnel_list = NULL;

void exit_handler(int maybe_unused signum) {
  LOG("Received signal %d, exiting...", signum);
  if (__tunnel_list) {
    tunnel_list_delete(__tunnel_list);
  }
  exit(0);
}

void print_stats_handler(int maybe_unused signum) {
  if (__tunnel_list) {
    // Print stats here
    for (size_t i = 0; i < tunnel_list_size(__tunnel_list); i++) {
      tunnel* t = tunnel_list_get(__tunnel_list, i);
      if (t) {
        tunnel_print_stats(t);
      }
    }
  }
}

typedef struct {
  int default_num_queues;
  bool seen_values;  // To know if we have seen any values for default section
  tunnel_params params;
  tunnel_list* tunnel_list;
} config_parse_state;

void reset_tunnel_params(tunnel_params* params) {
  free((void*)params->bind_iface);
  free((void*)params->allowed_remote_hosts);
  free((void*)params->default_remote_host);
  free((void*)params->bridge_name);
  free((void*)params->tunnel_name);
  memset(params, 0, sizeof(tunnel_params));
}

bool config_parse_event(const char* key, const char* value, void* user_data) {
  config_parse_state* parse_state = (config_parse_state*)user_data;
  tunnel_params* params = &parse_state->params;
  // Key value pair event
  if (value) {
    parse_state->seen_values = true;
    LOG_DEBUG("Key: %s, Value: %s", key, value);
    if (strcmp(key, "bind_iface") == 0) {
      params->bind_iface = strdup(value);
    } else if (strcmp(key, "bind_port") == 0) {
      params->bind_port = atoi(value);
    } else if (strcmp(key, "default_endpoint") == 0) {
      params->default_remote_host = strdup(value);
      char * colon = strchr(params->default_remote_host, ':');
      if (colon != NULL) {
        *colon = '\0';
        params->default_remote_port = atoi(colon + 1);
      } else {
        LOG("Invalid default endpoint format, expected <host>:<port>");
        return false;
      }
    } else if (strcmp(key, "num_queues") == 0) {
      params->num_queues = atoi(value);
    } else if (strcmp(key, "tap_base_name") == 0) {
      params->tap_base_name = strdup(value);
    } else if (strcmp(key, "bridge_name") == 0) {
      params->bridge_name = strdup(value);
    } else if (strcmp(key, "mtu") == 0) {
      params->mtu = atoi(value);
    } else if (strcmp(key, "allowed_remote_hosts") == 0) {
      params->allowed_remote_hosts = strdup(value);
    } else {
      LOG("Unknown key: %s", key);
      return false;
    }
  } else {
    // New section event, first commit the old tunnel
    if (parse_state->seen_values) {
      if (!params->tunnel_name) {
        params->tunnel_name = strdup("default");
      }
      LOG("Creating tunnel '%s'", params->tunnel_name);
      tunnel* t = tunnel_create(params);
      // Reset the tunnl object clearing any allocated resources
      if (!t) {
        LOG("Failed to create tunnel '%s'", params->tunnel_name);
        reset_tunnel_params(params);
        return false;
      }
      reset_tunnel_params(params);
      // Add the tunnel to the list
      if (!tunnel_list_add(parse_state->tunnel_list, t)) {
        LOG("Failed to add tunnel %s to list", params->tunnel_name);
        tunnel_delete(t);
        tunnel_list_delete(parse_state->tunnel_list);
        parse_state->tunnel_list = NULL;
        return false;
      }
    }
    if (key) {
      LOG_DEBUG("Section: %s", key);
      params->num_queues = parse_state->default_num_queues;
      params->tunnel_name = strdup(key);
    } else {
      // End of file
      LOG_DEBUG("End of file");
    }
  }
  return true;
}

void print_help_and_exit(char* exec_name) {
  fprintf(
      stderr,
      "Usage: %s <config_file>\n"
      "    config_file: <config_file> is the config file to use, which is an "
      "ini-style with the following format:\n"
      "```\n"
      "[<tunnel_name>]\n"
      "bind_iface=<bind_iface> ; The local interface to bind to "
      "listening/sending UDP packets\n"
      "                        ; Highly recommend that this is a secure "
      "interface such as wireguard\n"
      "bind_port=<bind_port> ; The local UDP port to bind to. Required\n"
      "allowed_remote_hosts=<allowed_remote_hosts> ; A comma separated list \n"
      "                                            ; of allowed remote hosts\n"
      "num_queues=<num_queues> ; The number of queues to use for the tunnel\n"
      "                        ; If not specified, the default is the number "
      "of cpu cores\n"
      "base_tap_name=<tap_name> ; The base name of the tap devices for this tunnel. Default is just 'tap'\n"
      "                         ; The created interfaces will have names tap0, tap1, etc.\n"
      "bridge_name=<bridge_name> ; The name of the bridge to add the tap "
      "devices to\n"
      "mtu=<mtu> ; The MTU to set for the tap devices, if not specified, the "
      "default is 1500\n"
      "```\n"
      "\n"
      "Note that for a single tunnel you can omit the section header\n"
      "Here is an example config file:\n"
      "```\n"
      "bind_iface=wg0 ; Assume in this example this interace has an ip of "
      "10.0.0.1\n"
      "bind_port=1234\n"
      "default_endpoint=10.0.0.1:1234\n"
      "bridge_name=br0\n"
      "```\n",
      exec_name);
  exit(1);
}

int main(int argc, char** argv) {
  if (argc != 2) {
    print_help_and_exit(argv[0]);
  }
  char* config_file = argv[1];
  if (config_file[0] == '-') {
    print_help_and_exit(argv[0]);
  }
  config_parse_state params = {
      .default_num_queues = sysconf(_SC_NPROCESSORS_ONLN),
      .tunnel_list = tunnel_list_create(),
  };
  if (!ini_parse_file(config_file, config_parse_event, &params)) {
    tunnel_list_delete(params.tunnel_list);
    return 1;
  }
  __tunnel_list = params.tunnel_list;
  signal(SIGINT, exit_handler);
  signal(SIGTERM, exit_handler);
  signal(SIGQUIT, exit_handler);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGUSR1, print_stats_handler);
  for (size_t i = 0; i < tunnel_list_size(params.tunnel_list); i++) {
    tunnel* t = tunnel_list_get(params.tunnel_list, i);
    if (t) {
      tunnel_wait(t);
    }
  }
  tunnel_list_delete(params.tunnel_list);
  return 0;
}
