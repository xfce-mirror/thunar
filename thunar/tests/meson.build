test_sources = [
  'test-resolve-symlink.c',
]

executable(
  'thunar-tests',
  test_sources,
  c_args: thunar_c_args,
  include_directories: [
    include_directories('../..'),
  ],
  dependencies: thunar_dependencies,
  link_with: [ libthunar, libthunarx ],
  install: false,
)

