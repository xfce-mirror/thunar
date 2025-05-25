shared_module(
  'thunar-sbr',
  [
    'thunar-sbr-case-renamer.c',
    'thunar-sbr-case-renamer.h',
    'thunar-sbr-date-renamer.c',
    'thunar-sbr-date-renamer.h',
    'thunar-sbr-enum-types.c',
    'thunar-sbr-enum-types.h',
    'thunar-sbr-insert-renamer.c',
    'thunar-sbr-insert-renamer.h',
    'thunar-sbr-number-renamer.c',
    'thunar-sbr-number-renamer.h',
    'thunar-sbr-plugin.c',
    'thunar-sbr-provider.c',
    'thunar-sbr-provider.h',
    'thunar-sbr-remove-renamer.c',
    'thunar-sbr-remove-renamer.h',
    'thunar-sbr-replace-renamer.c',
    'thunar-sbr-replace-renamer.h',
  ],
  name_prefix: '',
  gnu_symbol_visibility: 'hidden',
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('thunar-sbr'),
    '-D_XOPEN_SOURCE=700', # for strptime in time.h
  ],
  include_directories: [
    include_directories('..'),
    include_directories('..' / '..'),
  ],
  dependencies: [
    gexiv2,
    glib,
    gtk,
    libxfce4util,
    pcre2,
  ],
  link_with: [
    libthunarx,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('libdir') / 'thunarx-@0@'.format(thunarx_version_api),
)
