qt6 = import('qt6')
qt6_dep = dependency('qt6', modules: ['Core', 'Widgets', 'Network'], required: true)

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
