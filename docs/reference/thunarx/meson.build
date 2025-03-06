thunarx_doc_name = 'thunarx'

configure_file(
  configuration: configuration_data({
    'PACKAGE_NAME': '"@0@"'.format(thunarx_doc_name),
    'PACKAGE_VERSION': '"@0@"'.format(meson.project_version()),
    'PACKAGE_STRING': '"@0@"'.format('@0@-@1@'.format(thunarx_doc_name, meson.project_version())),
  }),
  input: 'gtkdocentities.ent.in',
  output: 'gtkdocentities.ent',
)

configure_file(
  configuration: configuration_data({
    'COPYRIGHT_YEAR': copyright_year,
  }),
  input: 'thunarx-docs.xml.in',
  output: 'thunarx-docs.xml',
  install: false,
)

gnome.gtkdoc(
  thunarx_doc_name,
  src_dir: 'thunarx',
  ignore_headers: [
    'thunarx-visibility.h',
  ],
  main_xml: meson.project_build_root() / 'docs' / 'reference' / 'thunarx' / 'thunarx-docs.xml',
  html_assets: [
    'images' / 'abstraction.png',
    'images' / 'bulk-rename.png',
    'images' / 'menu-provider.png',
    'images' / 'say-hello.png',
  ],
  dependencies: libthunarx_dep,
  include_directories: [
    meson.project_source_root(),
    meson.project_build_root(),
  ],
  gobject_typesfile: 'thunarx.types',
  install: true,
  check: false,
)
