executable(
  'thunar-sendto-email',
  [
    'main.c',
  ],
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('thunar-sendto-email'),
  ],
  include_directories: [
    include_directories('..' / '..'),
  ],
  dependencies: [
    glib,
    gtk,
    libxfce4ui,
    libxfce4util,
  ],
  install: true,
  install_dir: helper_path_prefix / 'Thunar',
)

desktop_in = configure_file(
  configuration: configuration_data({
    'HELPERDIR': helper_path_prefix,
  }),
  input: 'thunar-sendto-email.desktop.in.in',
  output: 'thunar-sendto-email.desktop.in',
  install: false,
)
i18n.merge_file(
  input: desktop_in,
  output: 'thunar-sendto-email.desktop',
  po_dir: '..' / '..' / 'po',
  type: 'desktop',
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / 'Thunar' / 'sendto',
)
