physfs = dependency('physfs', version: '>=2.1', static: build_static)
theora = dependency('theora', static: build_static)
vorbis = dependency('vorbis', static: build_static)
sdl3 = dependency('SDL3', static: build_static)
sdl3_ttf = dependency('SDL3_ttf', static: build_static)
sdl3_image = dependency('SDL3_image', static: build_static)
pixman = dependency('pixman-1', static: build_static)
png = dependency('libpng', static: build_static)
zlib = dependency('zlib', static: build_static)
uchardet = dependency('uchardet', static: build_static)
cppzmq = dependency('cppzmq', static: true)

if host_system == 'windows'
    bz2 = dependency('bzip2', static: build_static)
    iconv = compilers['cpp'].find_library('iconv', static: build_static)
else
    bz2 = compilers['cpp'].find_library('bz2')
    # FIXME: Specifically asking for static doesn't work if iconv isn't
    # installed in the system prefix somewhere
    iconv = compilers['cpp'].find_library('iconv')
    global_dependencies += compilers['cpp'].find_library('charset')
endif

# If OpenSSL is present, you get HTTPS support
if get_option('enable-https') == true
    openssl = dependency('openssl', required: false, static: build_static)
    if openssl.found() == true
        global_dependencies += openssl
        global_args += '-DMKXPZ_SSL'
        if host_system == 'windows'
            global_link_args += '-lcrypt32'
        endif
    else
        warning('Could not locate OpenSSL. HTTPS will be disabled.')
    endif
endif

global_args += '-DAL_ALEXT_PROTOTYPES'

# Windows needs to be treated like a special needs child here
explicit_libs = ''
if host_system == 'windows'
    # Newer versions of Ruby will refuse to link without these
    explicit_libs += 'libmsvcrt;libgcc;libmingwex;libgmp;'
endif
if build_static == true
    if host_system == 'windows'
        # '-static-libgcc', '-static-libstdc++' are here to avoid needing to ship a separate libgcc_s_seh-1.dll on Windows; it still works without those flags if you have the dll.
        global_link_args += ['-static-libgcc', '-static-libstdc++', '-Wl,-Bstatic', '-lgcc', '-lstdc++', '-lpthread', '-Wl,-Bdynamic']
    else
        global_link_args += ['-static-libgcc', '-static-libstdc++']
    endif
    global_args += '-DAL_LIBTYPE_STATIC'
endif

foreach l : explicit_libs.split(';')
        if l != ''
            global_link_args += '-l:' + l + '.a'
        endif
endforeach


if disable_openal == false
    vorbisfile = dependency('vorbisfile', static: build_static)
    ogg = dependency('ogg', static: build_static)
    openal = dependency('openal', static: build_static, method: 'pkg-config')
    sdl_sound = compilers['cpp'].find_library('SDL3_sound')

    global_dependencies += [vorbisfile, vorbis, ogg, openal, sdl_sound]

    alcdev_struct = 'ALCdevice_struct'
    if openal.type_name() == 'pkgconfig'
        if openal.version().version_compare('>=1.20.1')
            alcdev_struct = 'ALCdevice'
        endif
    endif

    global_args += '-DMKXPZ_ALCDEVICE=' + alcdev_struct

    if get_option('shared_fluid') == true
        fluidsynth = dependency('fluidsynth', static: build_static)
        add_project_arguments('-DSHARED_FLUID', language: 'cpp')
        global_dependencies += fluidsynth
        if host_system == 'windows'
            global_dependencies += compilers['cpp'].find_library('dsound')
        endif
    endif
endif


global_include_dirs += include_directories('.',
    'crypto',
    'display', 'display/gl', 'display/libnsgif', 'display/libnsgif/utils',
    'etc',
    'filesystem', 'filesystem/ghc',
    'input',
    'net',
    'system',
    'oneshot',
    'util', 'util/sigslot', 'util/sigslot/adapter'
)

if disable_openal == false
    global_include_dirs += include_directories('audio')
endif

global_dependencies += [zlib, bz2, sdl3, pixman, physfs, theora, sdl3_ttf, sdl3_image, png, iconv, uchardet, cppzmq, vorbis]
if host_system == 'windows'
    global_dependencies += compilers['cpp'].find_library('wsock32')
    global_dependencies += compilers['cpp'].find_library('winmm')
    global_dependencies += compilers['cpp'].find_library('Secur32')
    global_dependencies += compilers['cpp'].find_library('Shlwapi')
    global_dependencies += compilers['cpp'].find_library('gdiplus')
elif host_system == 'linux'
    global_dependencies += dependency('gtk+-3.0')
endif


if get_option('cjk_fallback_font') == true
    add_project_arguments('-DMKXPZ_CJK_FONT', language: 'cpp')
endif

main_source = files(
    'main.cpp',
    'config.cpp',
    'eventthread.cpp',
    'settingsmenu.cpp',
    'sharedstate.cpp',
    
    'crypto/rgssad.cpp',

    'display/autotiles.cpp',
    'display/autotilesvx.cpp',
    'display/bitmap.cpp',
    'display/font.cpp',
    'display/graphics.cpp',
    'display/plane.cpp',
    'display/sprite.cpp',
    'display/tilemap.cpp',
    'display/tilemapvx.cpp',
    'display/viewport.cpp',
    'display/window.cpp',
    'display/windowvx.cpp',

    'display/libnsgif/libnsgif.c',
    'display/libnsgif/lzw.c',

    'display/gl/gl-debug.cpp',
    'display/gl/gl-fun.cpp',
    'display/gl/gl-meta.cpp',
    'display/gl/glstate.cpp',
    'display/gl/scene.cpp',
    'display/gl/shader.cpp',
    'display/gl/texpool.cpp',
    'display/gl/tileatlas.cpp',
    'display/gl/tileatlasvx.cpp',
    'display/gl/tilequad.cpp',
    'display/gl/vertex.cpp',

    'util/iniconfig.cpp',
    'util/win-consoleutils.cpp',
    
    'etc/etc.cpp',
    'etc/table.cpp',

    'filesystem/filesystem.cpp',
    'filesystem/filesystemImpl.cpp',
    
    'input/input.cpp',
    'input/keybindings.cpp',

    'net/LUrlParser.cpp',
    'net/net.cpp',

    'system/systemImpl.cpp',

    'oneshot/oneshot.cpp',
    'oneshot/i18n.cpp',

    'theoraplay/theoraplay.c',
)

global_sources += main_source

audio_source = files(
    'audio/alstream.cpp',
    'audio/audio.cpp',
    'audio/audiostream.cpp',
    'audio/audiochannels.cpp',
    'audio/fluid-fun.cpp',
    'audio/midisource.cpp',
    'audio/sdlsoundsource.cpp',
    'audio/soundemitter.cpp',
    'audio/vorbissource.cpp',
)

if disable_openal == false
    global_sources += audio_source
endif

if host_system == 'linux'
    global_sources += files('oneshot/xdg-user-dir-lookup.c')
endif
