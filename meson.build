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
  subproject_dir: 'third-party',
  meson_version: '>=1.1.0',
)

include_dirs = include_directories('.')

thread_dep = dependency('threads')

# Header only third-party
add_project_arguments('-isystem' + meson.project_source_root()/'third-party/xxhash', language: 'cpp')
add_project_arguments('-isystem' + meson.project_source_root()/'third-party/function2/include', language: 'cpp')

# CMake based third-party
cmake = import('cmake')

default_defs = {'CMAKE_BUILD_TYPE' : 'Release'}

gtest_opts = cmake.subproject_options()
gtest_opts.add_cmake_defines(default_defs)
gtest_opts.set_install(false)
gtest_subproject = cmake.subproject('googletest', options: gtest_opts)
gtest_lib = gtest_subproject.dependency('gtest', include_type : 'system')
gmock_lib = gtest_subproject.dependency('gmock', include_type : 'system')
gtest_main_lib = gtest_subproject.dependency('gtest_main', include_type : 'system')

gbenchmark_opts = cmake.subproject_options()
gbenchmark_defs = default_defs + {'BENCHMARK_ENABLE_GTEST_TESTS' : false} + {'BENCHMARK_ENABLE_TESTING' : false}
gbenchmark_opts.add_cmake_defines(gbenchmark_defs)
gbenchmark_opts.set_install(false)
gbenchmark_subproject = cmake.subproject('benchmark', options: gbenchmark_opts)
gbenchmark_lib = gbenchmark_subproject.dependency('benchmark', include_type : 'system')

fmt_opts = cmake.subproject_options()
fmt_defs = default_defs + {'FMT_MASTER_PROJECT' : false}
fmt_opts.add_cmake_defines(fmt_defs)
fmt_opts.set_install(false)
fmt_subproject = cmake.subproject('fmt', options: fmt_opts)
fmt_lib = fmt_subproject.dependency('fmt', include_type : 'system')

# System based libs
boost_lib = dependency(
  'boost',
  modules : [
    'asio',
    'cobalt',
    'system',
    'thread',
  ],
  required : true
)

noctua = executable(
  'server',
  files(
    'tcp_example.cpp',
  ),
  dependencies: [
      fmt_lib,
      boost_lib,
      thread_dep,
  ],
  include_directories: include_dirs,
)

noctua_test = executable(
  'noctua-test',
  files(
    'test/message-queue-test.cpp',
    'test/sync-primitives-tests.cpp',
    'test/partition-test.cpp',
  ),
  dependencies: [
      fmt_lib,
      boost_lib,
      thread_dep,
      gtest_lib,
      gtest_main_lib,
  ],
  include_directories: include_dirs,
)

test(
  'noctua_tests',
  noctua_test,
  protocol: 'gtest',
  timeout: 120
)
