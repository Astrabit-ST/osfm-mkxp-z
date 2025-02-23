sdl3 = dependency('SDL3', static: true) # compilers['cpp'].find_library('SDL3', static: true)

embedded_assets = [
    'clover.png',
    'niko1.png',
    'niko2.png',
    'niko3.png'
]
embedded_assets_f = files(embedded_assets)

sources = []

count = 0
foreach file : embedded_assets_f
    sources += custom_target(embedded_assets[count],
    input: file,
    output: '@0@.xxd'.format(embedded_assets[count]),
    command: [
        xxd, '-i', '@INPUT@'
    ],
    capture: true,
    depend_files: embedded_assets_f[count]
    )
    count += 1
endforeach

sources += 'journal.cpp'
includes = [include_directories('stb', 'common')]

if host_system == 'linux'
    sources += '../src/oneshot/xdg-user-dir-lookup.c'
    includes += include_directories('../src/oneshot')
endif

if host_system == 'windows'
    win = import('windows')

    res = files(
        'resource.h',
        'icon.ico',
    )

    sources += win.compile_resources('resource.rc', depend_files: res)
endif

link_args = [
    '-static-libgcc', '-static-libstdc++', '-Wl,-Bstatic', '-lgcc', '-lstdc++', '-lpthread', 
]

if host_system == 'windows'
    link_args += ['-lws2_32', '-liphlpapi']
endif

executable(
    '_______',
    sources: sources,
    dependencies: [sdl3],
    link_args: link_args,
    win_subsystem: 'windows',
    include_directories: includes,
    cpp_args: args,
    objc_args: args,
    objcpp_args: args
)

global_include_dirs += include_directories('common')