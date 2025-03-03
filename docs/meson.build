custom_target(
  'Thunar.1',
  input: 'Thunar.xml',
  output: 'Thunar.1',
  command: [xsltproc, '--nonet', '--output', '@OUTPUT@', 'http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl', '@INPUT@'],
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / 'man' / 'man1',
)

install_data(
  'README.gtkrc',
  install_dir: get_option('prefix') / get_option('datadir') / 'doc' / 'thunar',
)
