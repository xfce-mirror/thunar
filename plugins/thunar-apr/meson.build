shared_module(
  'thunar-apr',
  [
    'thunar-apr-abstract-page.c',
    'thunar-apr-abstract-page.h',
    'thunar-apr-desktop-page.c',
    'thunar-apr-desktop-page.h',
    'thunar-apr-image-page.c',
    'thunar-apr-image-page.h',
    'thunar-apr-plugin.c',
    'thunar-apr-private.c',
    'thunar-apr-private.h',
    'thunar-apr-provider.c',
    'thunar-apr-provider.h',
  ],
  name_prefix: '',
  gnu_symbol_visibility: 'hidden',
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('thunar-apr'),
  ],
  include_directories: [
    include_directories('..'),
    include_directories('..' / '..'),
  ],
  dependencies: [
    glib,
    gtk,
    exo,
    libxfce4ui,
    libxfce4util,
    exif,
    libintl,
  ],
  link_with: [
    libthunarx,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('libdir') / 'thunarx-@0@'.format(thunarx_version_api),
)
