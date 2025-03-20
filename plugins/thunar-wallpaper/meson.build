shared_module(
  'thunar-wallpaper-plugin',
  [
    'thunar-wallpaper-plugin.c',
    'twp-provider.c',
    'twp-provider.h',
  ],
  name_prefix: '',
  gnu_symbol_visibility: 'hidden',
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('thunar-wallpaper-plugin'),
  ],
  include_directories: [
    include_directories('..' / '..'),
  ],
  dependencies: [
    glib,
    gtk,
    libxfce4util,
    xfconf,
    x11_deps,
    libintl,
  ],
  link_with: [
    libthunarx,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('libdir') / 'thunarx-@0@'.format(thunarx_version_api),
)
