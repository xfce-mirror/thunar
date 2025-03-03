shared_module(
  'tex-open-terminal',
  [
    'tex-open-terminal-plugin.c',
    'tex-open-terminal.c',
    'tex-open-terminal.h',
  ],
  name_prefix: '',
  gnu_symbol_visibility: 'hidden',
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('TexOpenTerminal'),
  ],
  include_directories: [
    include_directories('..'),
    include_directories('..' / '..'),
  ],
  dependencies: [
    glib,
    gtk,
    libxfce4ui,
  ],
  link_with: [
    libthunarx,
  ],
  install: false,
  # install_dir: get_option('prefix') / get_option('libdir') / 'thunarx-@0@'.format(thunarx_version_api),
)
