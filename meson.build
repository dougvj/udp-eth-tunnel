# Switch to gnu23 when more systems are updated
project('udp-eth-tunnel', 'c',
        default_options: ['c_std=gnu2x', 'warning_level=2', 'buildtype=debugoptimized', 'werror=true'])

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
libbsd = cc.find_library('bsd')
executable('udp-eth-tunnel', src, install: true, dependencies: [libbsd])

# Install the systemd service file
install_data('systemd/udp-eth-tunnel.service',
              install_dir: get_option('systemd_system_unitdir'),
              install_mode: 'r--r--r--')
