project(
  'noctua',
  'cpp',
  version: '0.0.0',
  default_options: [
    'cpp_std=gnu++2a',
    'warning_level=1',
    'buildtype=release',
    'optimization=3',
  ],
  subproject_dir: 'third_party',
  meson_version: '>=1.1.0',
)

include_dirs = include_directories('.')

thread_dep = dependency('threads')

gtest_dep = dependency('gtest', main: true, required: true)
benchmark_dep = dependency('benchmark', main: true, required: true)
