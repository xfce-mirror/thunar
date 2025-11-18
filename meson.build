project(
  'thunar',
  'c',
version : '4.21.3-dev',
  license : 'GPL-2.0-or-later',
  meson_version : '>= 0.61.0',
  default_options : ['c_std=gnu11', 'buildtype=debugoptimized', 'warning_level=2']
)

project_namespace = 'xfce'
pkgdatadir = get_option('prefix') / get_option('datadir') / meson.project_name()
copyright_year = '2025'
thunarx_version_api = '3'

cc = meson.get_compiler('c')
pkgconfig = import('pkgconfig')
gnome = import('gnome')
i18n = import('i18n')
fs = import('fs')

dependency_versions = {
  'glib': '>= 2.72.0',
  'gtk': '>= 3.24.0',
  'gdk-pixbuf': '>= 2.42.8',
  'libxfce4ui': '>= 4.21.3',
  'libxfce4util': '>= 4.17.2',
  'xfconf': '>= 4.12.0',
  'pango': '>= 1.38.0',

  'libx11': '>= 1.6.7',
  'libsm': '>= 1.2.3',
  'libice': '>= 1.0.10',

  'gir': '>= 1.72.0',
  'gudev': '>= 145',
  'libnotify': '>= 0.7.0',
  'polkit': '>= 0.102',
  'vte': '>= 0.70',

  'gexiv2': '>= 0.14.0',
  'pcre2': '>= 10.0',
  'libxfce4panel': '>= 4.14.0',
}

glib = dependency('glib-2.0', version: dependency_versions['glib'])
gio = dependency('gio-2.0', version: dependency_versions['glib'])
gthread = dependency('gthread-2.0', version: dependency_versions['glib'])
gmodule = dependency('gmodule-2.0', version: dependency_versions['glib'])
gtk = dependency('gtk+-3.0', version: dependency_versions['gtk'])
gdk_pixbuf = dependency('gdk-pixbuf-2.0', version: dependency_versions['gdk-pixbuf'])
libxfce4ui = dependency('libxfce4ui-2', version: dependency_versions['libxfce4ui'])
libxfce4kbd = dependency('libxfce4kbd-private-3', version: dependency_versions['libxfce4ui'])
libxfce4util = dependency('libxfce4util-1.0', version: dependency_versions['libxfce4util'])
xfconf = dependency('libxfconf-0', version: dependency_versions['xfconf'])
pango = dependency('pango', version: dependency_versions['pango'])

xdt_gen_visibility = find_program('xdt-gen-visibility', required: true)
xsltproc = find_program('xsltproc', required: true)

x11_deps = []
x11_deps += dependency('gdk-x11-3.0', version: dependency_versions['gtk'], required: get_option('x11'))
x11_deps += dependency('x11', version: dependency_versions['libx11'], required: get_option('x11'))

feature_cflags = []

enable_x11 = not get_option('x11').disabled()
foreach dep : x11_deps
  enable_x11 = enable_x11 and dep.found()
endforeach
if enable_x11
  feature_cflags += '-DENABLE_X11=1'

  libsm = dependency('sm', version: dependency_versions['libsm'], required: get_option('session-management'))
  if libsm.found()
    libice = dependency('ice', version: dependency_versions['libice'], required: get_option('session-management'))
    if libice.found()
      feature_cflags += '-DENABLE_LIBSM=1'
      x11_deps += [libsm, libice]
    endif
  endif
endif

gio_unix = dependency('gio-unix-2.0', version: dependency_versions['glib'], required: get_option('gio-unix'))
if gio_unix.found()
  feature_cflags += '-DHAVE_GIO_UNIX=1'
endif

vte = dependency('vte-2.91', required: get_option('terminal'))
if vte.found()
  feature_cflags += '-DHAVE_VTE=1'
endif

gudev = dependency('gudev-1.0', version: dependency_versions['gudev'], required: get_option('gudev'))
if gudev.found()
  feature_cflags += '-DHAVE_GUDEV=1'
endif

libnotify = dependency('libnotify', version: dependency_versions['libnotify'], required: get_option('libnotify'))
if libnotify.found()
  feature_cflags += '-DHAVE_LIBNOTIFY=1'
endif

polkit = dependency('polkit-gobject-1', version: dependency_versions['polkit'], required: get_option('polkit'))
gir = dependency('gobject-introspection-1.0', version: dependency_versions['gir'], required: get_option('introspection'))

gexiv2 = dependency('gexiv2', version: dependency_versions['gexiv2'], required: get_option('gexiv2'))
if gexiv2.found()
  feature_cflags += '-DHAVE_GEXIV2=1'
endif

pcre2 = dependency('libpcre2-8', version: dependency_versions['pcre2'], required: get_option('pcre2'))
if pcre2.found()
  feature_cflags += '-DHAVE_PCRE2=1'
endif

libxfce4panel = dependency('libxfce4panel-2.0', version: dependency_versions['libxfce4panel'], required: get_option('thunar-tpa'))

gnu_symbol_visibility = 'default'
if get_option('visibility')
  gnu_symbol_visibility = 'hidden'
  feature_cflags += '-DENABLE_SYMBOL_VISIBILITY=1'
endif

helper_path_prefix = get_option('helper-path-prefix')
if helper_path_prefix == ''
  helper_path_prefix = get_option('prefix') / get_option('libdir')
endif

thunarx_dirs = get_option('thunarx-dirs')
if thunarx_dirs == ''
  thunarx_dirs = get_option('prefix') / get_option('libdir') / 'thunarx-@0@'.format(thunarx_version_api)
endif

if cc.has_function('bind_textdomain_codeset')
  feature_cflags += '-DHAVE_BIND_TEXTDOMAIN_CODESET=1'
  libintl = dependency('', required: false)
else
  libintl = cc.find_library('intl', required: false)
  if libintl.found() and cc.has_function('bind_textdomain_codeset', dependencies: libintl)
    feature_cflags += '-DHAVE_BIND_TEXTDOMAIN_CODESET=1'
  endif
endif
  
functions = [
  'atexit',
  'mkdtemp',
  'setgroupent',
  'setpassent',
  'statx',
  'strptime',
]
foreach function : functions
  if cc.has_function(function)
    feature_cflags += '-DHAVE_@0@=1'.format(function.to_upper())
  endif
endforeach

headers = [
  'sys/param.h',
  'sys/stat.h',
  'sys/types.h',
  'sys/wait.h',
  'errno.h',
  'fcntl.h',
  'grp.h',
  'libintl.h',
  'limits.h',
  'locale.h',
  'malloc.h',
  'memory.h',
  'paths.h',
  'pwd.h',
  'signal.h',
  'stdarg.h',
  'stdio.h',
  'stdlib.h',
  'string.h',
  'strings.h',
  'time.h',
  'unistd.h',
]
foreach header : headers
  if cc.check_header(header)
    feature_cflags += '-DHAVE_@0@=1'.format(header.underscorify().to_upper())
  endif
endforeach

extra_cflags = []
extra_cflags_check = [
  '-Wmissing-declarations',
  '-Wmissing-noreturn',
  '-Wold-style-definition',
  '-Wredundant-decls',
  '-Wpointer-arith',
  '-Wcast-align',
  '-Winit-self',
  '-Wshadow',
  '-Wmissing-include-dirs',
  '-Wundef',
  '-Wformat',
  '-Wformat-security',
  '-Wformat-y2k',
  '-Wnested-externs',
  '-Wno-unused-parameter',
  '-Wno-declaration-after-statement',
  '-Wno-missing-field-initializers',
  '-Werror=implicit-function-declaration',
  '-Wno-error=deprecated-declarations',
]

optimization = get_option('optimization')
if get_option('debug') and optimization in ['0', 'g']
  extra_cflags_check += '-fstack-protector-strong'
  extra_cflags += [
    '-DDEBUG=1',
    '-DDEBUG_TRACE=1',
    '-DG_ENABLE_DEBUG',
  ]
elif optimization in ['3', 'minsize']
  extra_cflags += [
    '-DNDEBUG',
    '-DG_DISABLE_CAST_CHECKS',
    '-DG_DISABLE_ASSERT',
  ]
endif

if dependency_versions.has_key('glib')
  glib_version_parts = dependency_versions['glib'].split(' ')
  glib_min_version_parts = glib_version_parts[1].split('.')
  glib_min_version_define = 'GLIB_VERSION_@0@_@1@'.format(glib_min_version_parts[0], glib_min_version_parts[1])
  extra_cflags += [
    '-DGLIB_VERSION_MIN_REQUIRED=@0@'.format(glib_min_version_define),
    '-DGLIB_VERSION_MAX_ALLOWED=@0@'.format(glib_min_version_define),
    '-DG_LOG_USE_STRUCTURED=1',
  ]
endif

version_parts = meson.project_version().split('-dev')[0].split('.')
version_short = '@0@.@1@'.format(version_parts[0], version_parts[1])

extra_cflags += [
  '-DPACKAGE="@0@"'.format(meson.project_name()),
  '-DPACKAGE_NAME="@0@"'.format(meson.project_name()),
  '-DPACKAGE_VERSION="@0@"'.format(meson.project_version()),
  '-DVERSION="@0@"'.format(meson.project_version()),
  '-DVERSION_SHORT="@0@"'.format(version_short),
  '-DPACKAGE_STRING="@0@ @1@"'.format(meson.project_name(), meson.project_version()),
  '-DPACKAGE_DATADIR="@0@"'.format(pkgdatadir),
  '-DCOPYRIGHT_YEAR="@0@"'.format(copyright_year),
  '-DPACKAGE_LOCALE_DIR="@0@"'.format(get_option('prefix') / get_option('localedir')),
  '-DPACKAGE_BUGREPORT="https://gitlab.xfce.org/@0@/@1@/-/issues"'.format(project_namespace, meson.project_name()),
  '-DGETTEXT_PACKAGE="@0@"'.format(meson.project_name()),
  '-DPREFIX="@0@"'.format(get_option('prefix')),
  '-DBINDIR="@0@"'.format(get_option('prefix') / get_option('bindir')),
  '-DDATADIR="@0@"'.format(get_option('prefix') / get_option('datadir')),
  '-DINCLUDEDIR="@0@"'.format(get_option('prefix') / get_option('includedir')),
  '-DLIBDIR="@0@"'.format(get_option('prefix') / get_option('libdir')),
  '-DLIBEXECDIR="@0@"'.format(get_option('prefix') / get_option('libexecdir')),
  '-DLOCALEDIR="@0@"'.format(get_option('prefix') / get_option('localedir')),
  '-DLOCALSTATEDIR="@0@"'.format(get_option('prefix') / get_option('localstatedir')),
  '-DSBINDIR="@0@"'.format(get_option('prefix') / get_option('sbindir')),
  '-DSYSCONFDIR="@0@"'.format(get_option('prefix') / get_option('sysconfdir')),
  '-DHAVE_XFCE_REVISION_H=1',
]

add_project_arguments(cc.get_supported_arguments(extra_cflags_check), language: 'c')
add_project_arguments(extra_cflags, language: 'c')
add_project_arguments(feature_cflags, language: 'c')

xfce_revision_h = vcs_tag(
  command: ['git', 'rev-parse', '--short', 'HEAD'],
  fallback: 'UNKNOWN',
  input: 'xfce-revision.h.in',
  output: 'xfce-revision.h',
  replace_string: '@REVISION@',
)

service_files = [
  'org.xfce.FileManager.service.in',
  'org.xfce.Thunar.FileManager1.service.in',
  'org.xfce.Thunar.service.in',
  'thunar.service.in',
]
foreach file : service_files
  if file == 'thunar.service.in'
    install_dir = get_option('prefix') / 'lib' / 'systemd' / 'user'
  else
    install_dir = get_option('prefix') / get_option('datadir') / 'dbus-1' / 'services'
  endif

  configure_file(
    configuration: configuration_data({
      'bindir': get_option('prefix') / get_option('bindir'),
    }),
    input: file,
    output: fs.stem(file),
    install: true,
    install_dir: install_dir,
  )
endforeach

desktop_files = [
  'thunar-bulk-rename.desktop.in',
  'thunar.desktop.in',
]
foreach file : desktop_files
  i18n.merge_file(
    input: file,
    output: fs.stem(file),
    po_dir: 'po',
    type: 'desktop',
    install: true,
    install_dir: get_option('prefix') / get_option('datadir') / 'applications',
  )
endforeach

if polkit.found()
  thunar_policy_in = configure_file(
    configuration: configuration_data({
      'bindir': get_option('prefix') / get_option('bindir'),
    }),
    input: 'org.xfce.thunar.policy.in.in',
    output: 'org.xfce.thunar.policy.in',
    install: false,
  )
  i18n.merge_file(
    input: thunar_policy_in,
    output: 'org.xfce.thunar.policy',
    po_dir: 'po',
    type: 'xml',
    install: true,
    install_dir: get_option('prefix') / get_option('datadir') / 'polkit-1' / 'actions',
  )
endif

i18n.merge_file(
  input: 'org.xfce.thunar.appdata.xml.in',
  output: 'org.xfce.thunar.appdata.xml',
  po_dir: 'po',
  type: 'xml',
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / 'metainfo',
)

subdir('docs')
subdir('thunarx')
if get_option('gtk-doc')
  subdir('docs' / 'reference' / 'thunarx')
endif
subdir('examples' / 'tex-open-terminal')
subdir('icons')
if not get_option('thunar-apr').disabled()
  subdir('plugins' / 'thunar-apr')
endif
if not get_option('thunar-sbr').disabled()
  subdir('plugins' / 'thunar-sbr')
endif
subdir('plugins' / 'thunar-sendto-email')
if not get_option('thunar-tpa').disabled() and libxfce4panel.found()
  subdir('plugins' / 'thunar-tpa')
endif
if not get_option('thunar-uca').disabled()
  subdir('plugins' / 'thunar-uca')
endif
if not get_option('thunar-wallpaper').disabled()
  subdir('plugins' / 'thunar-wallpaper')
endif
subdir('po')
subdir('thunar')
