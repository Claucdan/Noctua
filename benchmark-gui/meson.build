qt6 = import('qt6')
qt6_dep = dependency('qt6', modules: ['Core', 'Widgets', 'Network'], required: true)
qt6_test_dep = dependency('qt6', modules: ['Core', 'Widgets', 'Network', 'Test'], required: true)

include_dirs = include_directories('..', '../src')

app_moc_sources = qt6.preprocess(
    moc_headers: files(
        'src/main-window.h',
        'src/worker.h',
        'src/writer-worker.h',
        'src/reader-worker.h',
        'src/benchmark-runner.h'
    )
)

sources = files(
    'src/main.cpp',
    'src/main-window.cpp',
    'src/worker.cpp',
    'src/writer-worker.cpp',
    'src/reader-worker.cpp',
    'src/benchmark-runner.cpp',
    'src/protocol-utils.cpp'
)

executable('noctua-benchmark',
    sources,
    app_moc_sources,
    dependencies: qt6_dep,
    include_directories: include_dirs,
    install: true
)

test_support_moc_sources = qt6.preprocess(
    moc_headers: files(
        'src/worker.h',
        'src/writer-worker.h',
        'src/reader-worker.h',
        'src/benchmark-runner.h',
        'test/benchmark-gui-qt-test.h',
    )
)

benchmark_gui_test = executable(
    'noctua-benchmark-test',
    files(
        'test/benchmark-gui-qt-test.cpp',
        'src/worker.cpp',
        'src/writer-worker.cpp',
        'src/reader-worker.cpp',
        'src/benchmark-runner.cpp',
        'src/protocol-utils.cpp',
    ),
    test_support_moc_sources,
    dependencies: [
        qt6_test_dep,
        boost_lib,
        thread_dep,
        fmt_lib,
    ],
    include_directories: [
        include_dirs,
        include_directories('..'),
        include_directories('src'),
    ],
)

test(
    'noctua_benchmark_tests',
    benchmark_gui_test,
    timeout: 120,
)
