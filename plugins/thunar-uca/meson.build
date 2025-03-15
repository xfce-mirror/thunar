plugin_sources = [
  'thunar-uca-chooser.c',
  'thunar-uca-chooser.h',
  'thunar-uca-context.c',
  'thunar-uca-context.h',
  'thunar-uca-editor.c',
  'thunar-uca-editor.h',
  'thunar-uca-model.c',
  'thunar-uca-model.h',
  'thunar-uca-plugin.c',
  'thunar-uca-private.c',
  'thunar-uca-private.h',
  'thunar-uca-provider.c',
  'thunar-uca-provider.h',
]

plugin_sources += gnome.compile_resources(
  'thunar-uca.gresource',
  'thunar-uca.gresource.xml',
)

shared_module(
  'thunar-uca',
  plugin_sources,
  name_prefix: '',
  gnu_symbol_visibility: 'hidden',
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('thunar-uca'),
  ],
  include_directories: [
    include_directories('..'),
    include_directories('..' / '..'),
  ],
  dependencies: [
    glib,
    gtk,
    libxfce4ui,
    libxfce4kbd,
    libxfce4util,
    libintl,
  ],
  link_with: [
    libthunarx,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('libdir') / 'thunarx-@0@'.format(thunarx_version_api),
)

i18n.merge_file(
  input: 'uca.xml.in',
  output: 'uca.xml',
  data_dirs: '..' / '..',
  po_dir: '..' / '..' / 'po',
  type: 'xml',
  install: true,
  install_dir: get_option('prefix') / get_option('sysconfdir') / 'xdg' / 'Thunar',
)
