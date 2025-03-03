plugin_sources = [
  'thunar-tpa.c',
]

plugin_sources += gnome.gdbus_codegen(
  'thunar-tpa-bindings',
  sources: 'thunar-tpa-bindings.xml',
  interface_prefix: 'org.xfce',
  namespace: 'thunarTPA',
  install_header: false,
)

plugin_install_subdir = 'xfce4' / 'panel' / 'plugins'

shared_module(
  'thunar-tpa',
  plugin_sources,
  gnu_symbol_visibility: 'hidden',
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('libthunar-tpa'),
  ],
  include_directories: [
    include_directories('..'),
    include_directories('..' / '..'),
  ],
  dependencies: [
    glib,
    gtk,
    libxfce4ui,
    libxfce4util,
    libxfce4panel,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('libdir') / plugin_install_subdir,
)

i18n.merge_file(
  input: 'thunar-tpa.desktop.in',
  output: 'thunar-tpa.desktop',
  po_dir: '..' / '..' / 'po',
  type: 'desktop',
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / plugin_install_subdir,
)
