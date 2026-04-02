qt6 = import('qt6')
qt6_dep = dependency('qt6', modules: ['Core', 'Widgets', 'Network'], required: true)
qt6_test_dep = dependency('qt6', modules: ['Core', 'Network', 'Test'], required: true)

include_dirs = include_directories('../src')

moc_sources = qt6.preprocess(
    moc_headers: files(
        'src/mainwindow.h',
        'src/worker.h',
        'src/writerworker.h',
        'src/readerworker.h',
        'src/benchmarkrunner.h'
    )
)

sources = files(
    'src/main.cpp',
    'src/mainwindow.cpp',
    'src/worker.cpp',
    'src/writerworker.cpp',
    'src/readerworker.cpp',
    'src/benchmarkrunner.cpp',
    'src/protocolutils.cpp'
)

executable('noctua-benchmark',
    sources,
    moc_sources,
    dependencies: qt6_dep,
    include_directories: include_dirs,
    install: true
)

test_moc_sources = qt6.preprocess(
    moc_headers: files('test/benchmark-gui-qt-test.h')
)

benchmark_gui_test = executable(
    'noctua-benchmark-test',
    files(
        'test/benchmark-gui-qt-test.cpp',
        'src/worker.cpp',
        'src/writerworker.cpp',
        'src/readerworker.cpp',
        'src/benchmarkrunner.cpp',
        'src/protocolutils.cpp',
    ),
    moc_sources,
    test_moc_sources,
    dependencies: [
        qt6_test_dep,
        boost_lib,
        thread_dep,
        fmt_lib,
    ],
    include_directories: [
        include_dirs,
        include_directories('src'),
    ],
)

test(
    'noctua_benchmark_tests',
    benchmark_gui_test,
    timeout: 120,
)
