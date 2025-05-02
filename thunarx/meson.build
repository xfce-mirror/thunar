libthunarx_libname = 'thunarx'
libthunarx_pkgname = '@0@-@1@'.format(libthunarx_libname, thunarx_version_api)

libthunarx_install_subdir = libthunarx_pkgname / libthunarx_libname
libthunarx_install_dir = get_option('prefix') / get_option('includedir') / libthunarx_install_subdir

libthunarx_headers = [
  'thunarx-file-info.h',
  'thunarx-menu-item.h',
  'thunarx-menu-provider.h',
  'thunarx-menu.h',
  'thunarx-preferences-provider.h',
  'thunarx-property-page-provider.h',
  'thunarx-property-page.h',
  'thunarx-provider-factory.h',
  'thunarx-provider-module.h',
  'thunarx-provider-plugin.h',
  'thunarx-renamer-provider.h',
  'thunarx-renamer.h',
  'thunarx.h',
]

libthunarx_headers += configure_file(
  configuration: configuration_data({
    'THUNAR_VERSION_MAJOR': version_parts[0],
    'THUNAR_VERSION_MINOR': version_parts[1],
    'THUNAR_VERSION_MICRO': version_parts[2],
  }),
  input: 'thunarx-config.h.in',
  output: 'thunarx-config.h',
  install: false,
)

libthunarx_sources = [
  'thunarx-config.c',
  'thunarx-file-info.c',
  'thunarx-menu-item.c',
  'thunarx-menu-provider.c',
  'thunarx-menu.c',
  'thunarx-preferences-provider.c',
  'thunarx-private.c',
  'thunarx-private.h',
  'thunarx-property-page-provider.c',
  'thunarx-property-page.c',
  'thunarx-provider-factory.c',
  'thunarx-provider-module.c',
  'thunarx-provider-plugin.c',
  'thunarx-renamer-provider.c',
  'thunarx-renamer.c',
]

libthunarx_sources += configure_file(
  input: 'thunarx.symbols',
  output: 'thunarx-visibility.h',
  command: [xdt_gen_visibility, '--kind=header', '@INPUT@', '@OUTPUT@'],
)
libthunarx_sources += configure_file(
  input: 'thunarx.symbols',
  output: 'thunarx-visibility.c',
  command: [xdt_gen_visibility, '--kind=source', '@INPUT@', '@OUTPUT@'],
)

libthunarx_deps = [
  glib,
  gio,
  gmodule,
  gtk,
  libxfce4util,
]

libthunarx_so_version = '0'
libthunarx_version = '@0@.0.0'.format(libthunarx_so_version)

libthunarx = library(
  libthunarx_pkgname,
  libthunarx_sources,
  version: libthunarx_version,
  soversion: libthunarx_so_version,
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format(libthunarx_libname),
    '-DTHUNARX_COMPILATION=1',
    '-DTHUNARX_DIRECTORY="@0@"'.format(thunarx_dirs),
    '-DTHUNARX_ENABLE_CUSTOM_DIRS="@0@"'.format(get_option('thunarx-dirs-envvar') ? 'TRUE' : 'FALSE'),
  ],
  include_directories: [
    include_directories('..'),
  ],
  gnu_symbol_visibility: gnu_symbol_visibility,
  dependencies: libthunarx_deps,
  install: true,
)

install_headers(
  libthunarx_headers,
  subdir: libthunarx_install_subdir,
)

pkgconfig.generate(
  libraries: libthunarx,
  version: meson.project_version(),
  name: libthunarx_libname,
  filebase: libthunarx_pkgname,
  description: 'A library to create Thunar extensions',
  subdirs: [
    libthunarx_pkgname,
  ],
  variables: [
    'extensionsdir=@0@'.format('${libdir}' / libthunarx_pkgname),
  ],
  install_dir: get_option('prefix') / get_option('libdir') / 'pkgconfig',
)

if get_option('introspection')
  gnome.generate_gir(
    libthunarx,
    sources: libthunarx_sources + libthunarx_headers,
    namespace: 'Thunarx',
    nsversion: '@0@.0'.format(thunarx_version_api),
    identifier_prefix: [
      'Thunarx',
    ],
    symbol_prefix: [
      'thunarx',
    ],
    export_packages: libthunarx_pkgname,
    includes: [
      'GLib-2.0',
      'GObject-2.0',
      'Gio-2.0',
      'Gtk-3.0',
      'Libxfce4util-1.0',
    ],
    header: 'thunarx' / 'thunarx.h',
    install: true,
    extra_args: [
      '-DTHUNARX_COMPILATION=1',
    ],
  )
endif

libthunarx_dep = declare_dependency(
  dependencies: libthunarx_deps,
  link_with: libthunarx,
)
