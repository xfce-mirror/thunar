test_sources = [
  'test-resolve-symlink.c',
]

# Prefix each entry in 'thunar_sources_base' with "../" to be able to locate the files
thunar_sources_prefixed = []
foreach source : thunar_sources_base
  if source != 'main.c' # we use our own testing-main instead
    thunar_sources_prefixed += ['../' + source]
  endif
endforeach

test_sources += thunar_sources_prefixed

executable(
  'thunar-tests',
  test_sources + gnome_marshal_sources + gnome_gdbus_sources + gnome_compile_sources,
  c_args: thunar_c_args,
  include_directories: [
    include_directories('../..'),
  ],
  dependencies: thunar_dependencies,
  link_with: [ libthunarx ],
  install: false,
)

