# Switch to gnu23 when more systems are updated
project('udp-eth-tunnel', 'c',
        version: '2025.5.26',
        default_options: ['c_std=gnu99', 'warning_level=2', 'buildtype=release', 'werror=true'])

src = [
  'src/main.c',
  'src/ini.c',
  'src/ip_addr.c',
  'src/endpoint_map.c',
  'src/main.c',
  'src/net.c',
  'src/tunnel.c',
]
if get_option('debug')
  add_project_arguments('-DDEBUG_ENABLED', language: 'c')
endif
cc = meson.get_compiler('c')
# Some newer versions of glibc have strlcpy and strlcat but
# for compatibility with older versions we need to use libbsd
# which provides these functions.
deps = []
if not cc.has_function('strlcpy', prefix: '#include <string.h>') or not cc.has_function('strlcat', prefix: '#include <string.h>')
  warning('strlcpy and strlcat not found, using libbsd')
  libbsd = cc.find_library('bsd')
  deps += [libbsd]
  add_project_arguments('-DUSE_LIBBSD', language: 'c')
endif
executable('udp-eth-tunnel', src, install: true, dependencies: deps)

# Install the systemd service file
install_data('systemd/udp-eth-tunnel.service',
              install_dir: get_option('systemd_system_unitdir'),
              install_mode: 'r--r--r--')
