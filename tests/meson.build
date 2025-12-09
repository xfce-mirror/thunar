test_bins = [
  'test-resolve-symlink',
]

foreach bin : test_bins
  e = executable(
    bin,
    sources: [
      '@0@.c'.format(bin),
    ],
    c_args: thunar_c_args + '-UG_DISABLE_ASSERT',
    include_directories: [
      include_directories('..'),
    ],
    dependencies: thunar_dependencies,
    link_with: [ libthunar, libthunarx ],
    install: false,
  )

  test(bin, e)
endforeach
