ARCH := $(shell ruby -e "puts ENV[\"MSYSTEM\"].downcase")
RB_PREFIX := $(shell ruby -e "case '$(ARCH)'; \
                when 'mingw64'; \
                  puts 'x64-msvcrt'; \
                when 'mingw32'; \
                  puts 'msvcrt'; \
                when 'ucrt64', 'clang64'; \
                  puts 'x64-ucrt'; \
                when 'clang32'; \
                  puts 'ucrt'; \
                end \
              ")
SDL_FLAGS := ${SDL_FLAGS}
SDL3_IMAGE_FLAGS := ${SDL3_IMAGE_FLAGS}
SDL3_TTF_FLAGS := ${SDL3_TTF_FLAGS}
OPENAL_FLAGS := ${OPENAL_FLAGS}
OPENSSL_FLAGS := mingw64 ${OPENSSL_FLAGS}
RUBY_FLAGS := ${RUBY_FLAGS}

BUILD_PREFIX := ${PWD}/build-$(ARCH)
LIBDIR := $(BUILD_PREFIX)/lib
BINDIR := $(BUILD_PREFIX)/bin
INCLUDEDIR := $(BUILD_PREFIX)/include
DOWNLOADS := ${PWD}/downloads/$(ARCH)
NPROC := $(shell nproc)
CFLAGS := -I$(INCLUDEDIR) -O3
LDFLAGS := -L$(LIBDIR)
CC      := gcc
PKG_CONFIG_LIBDIR := $(BUILD_PREFIX)/lib/pkgconfig
GIT := git
CLONE := $(GIT) clone -q
GITHUB := https://github.com

CONFIGURE_ENV := \
	PKG_CONFIG_LIBDIR=$(PKG_CONFIG_LIBDIR) \
	CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"

CONFIGURE_ARGS := --prefix="$(BUILD_PREFIX)"

CMAKE_ARGS := \
	-DCMAKE_INSTALL_PREFIX="$(BUILD_PREFIX)" \
	-DCMAKE_PREFIX_PATH="$(BUILD_PREFIX)" \
	-DCMAKE_C_FLAGS="$(CFLAGS)" \
	-DCMAKE_BUILD_TYPE=Release \
	-G "MSYS Makefiles"

RUBY_CONFIGURE_ARGS := \
	--prefix="$(shell cygpath -m ${BUILD_PREFIX})" \
	--without-gmp \
	--disable-install-doc 
#	--enable-install-static-library \
	--with-out-ext=openssl,readline,dbm,gdbm,win32ole \
	--with-static-linked-ext \
	--disable-rubygems \

CONFIGURE := $(CONFIGURE_ENV) ./configure $(CONFIGURE_ARGS)
AUTOGEN   := $(CONFIGURE_ENV) ./autogen.sh $(CONFIGURE_ARGS)
CMAKE     := $(CONFIGURE_ENV) cmake .. $(CMAKE_ARGS)

default: everything

# Theora
libtheora: init_dirs libvorbis libogg $(LIBDIR)/libtheora.a

$(LIBDIR)/libtheora.a: $(LIBDIR)/libogg.a $(DOWNLOADS)/theora/Makefile
	cd $(DOWNLOADS)/theora; \
	make -j$(NPROC); make install

$(DOWNLOADS)/theora/Makefile: $(DOWNLOADS)/theora/configure
	cd $(DOWNLOADS)/theora; \
	$(CONFIGURE) --with-ogg=$(BUILD_PREFIX) --enable-shared=false --enable-static=true --disable-examples

$(DOWNLOADS)/theora/configure: $(DOWNLOADS)/theora/autogen.sh
	cd $(DOWNLOADS)/theora; \
	./autogen.sh

$(DOWNLOADS)/theora/autogen.sh:
	$(CLONE) $(GITHUB)/xiph/theora $(DOWNLOADS)/theora

# Vorbis
libvorbis: init_dirs libogg $(LIBDIR)/libvorbis.a

$(LIBDIR)/libvorbis.a: $(LIBDIR)/libogg.a $(DOWNLOADS)/vorbis/Makefile
	cd $(DOWNLOADS)/vorbis; \
	make -j$(NPROC); make install

$(DOWNLOADS)/vorbis/Makefile: $(DOWNLOADS)/vorbis/configure
	cd $(DOWNLOADS)/vorbis; \
	$(CONFIGURE) --with-ogg=$(BUILD_PREFIX) --enable-shared=false --enable-static=true

$(DOWNLOADS)/vorbis/configure: $(DOWNLOADS)/vorbis/autogen.sh
	cd $(DOWNLOADS)/vorbis; \
	./autogen.sh

$(DOWNLOADS)/vorbis/autogen.sh:
	$(CLONE) $(GITHUB)/mkxp-z/vorbis $(DOWNLOADS)/vorbis


# Ogg, dependency of Vorbis
libogg: init_dirs $(LIBDIR)/libogg.a

$(LIBDIR)/libogg.a: $(DOWNLOADS)/ogg/Makefile
	cd $(DOWNLOADS)/ogg; \
	make -j$(NPROC); make install

$(DOWNLOADS)/ogg/Makefile: $(DOWNLOADS)/ogg/configure
	cd $(DOWNLOADS)/ogg; \
	$(CONFIGURE) --enable-static=true --enable-shared=false

$(DOWNLOADS)/ogg/configure: $(DOWNLOADS)/ogg/autogen.sh
	cd $(DOWNLOADS)/ogg; ./autogen.sh

$(DOWNLOADS)/ogg/autogen.sh:
	$(CLONE) $(GITHUB)/mkxp-z/ogg $(DOWNLOADS)/ogg

# Pixman
pixman: init_dirs libpng $(LIBDIR)/libpixman-1.a

$(LIBDIR)/libpixman-1.a: $(DOWNLOADS)/pixman/Makefile
	cd $(DOWNLOADS)/pixman
	make -C $(DOWNLOADS)/pixman -j$(NPROC)
	make -C $(DOWNLOADS)/pixman install

$(DOWNLOADS)/pixman/Makefile: $(DOWNLOADS)/pixman/autogen.sh
	cd $(DOWNLOADS)/pixman; \
	$(AUTOGEN) --enable-static=yes --enable-shared=no

$(DOWNLOADS)/pixman/autogen.sh:
	$(CLONE) $(GITHUB)/mkxp-z/pixman $(DOWNLOADS)/pixman


# PhysFS

physfs: init_dirs $(LIBDIR)/libphysfs.a

$(LIBDIR)/libphysfs.a: $(DOWNLOADS)/physfs/cmakebuild/Makefile
	cd $(DOWNLOADS)/physfs/cmakebuild; \
	make -j$(NPROC); make install

$(DOWNLOADS)/physfs/cmakebuild/Makefile: $(DOWNLOADS)/physfs/CMakeLists.txt
	cd $(DOWNLOADS)/physfs; \
	mkdir cmakebuild; cd cmakebuild; \
	$(CMAKE) -DPHYSFS_BUILD_STATIC=true -DPHYSFS_BUILD_SHARED=false

$(DOWNLOADS)/physfs/CMakeLists.txt:
	$(CLONE) $(GITHUB)/mkxp-z/physfs -b release-3.2.0 $(DOWNLOADS)/physfs

# libpng
libpng: init_dirs $(LIBDIR)/libpng.a

$(LIBDIR)/libpng.a: $(DOWNLOADS)/libpng/Makefile
	cd $(DOWNLOADS)/libpng; \
	make -j$(NPROC); make install

$(DOWNLOADS)/libpng/Makefile: $(DOWNLOADS)/libpng/configure
	cd $(DOWNLOADS)/libpng; \
	$(CONFIGURE) \
	--enable-shared=no --enable-static=yes

$(DOWNLOADS)/libpng/configure:
	$(CLONE) $(GITHUB)/mkxp-z/libpng $(DOWNLOADS)/libpng

# uchardet
uchardet: init_dirs $(LIBDIR)/libuchardet.a

$(LIBDIR)/libuchardet.a: $(DOWNLOADS)/uchardet/cmakebuild/Makefile
	cd $(DOWNLOADS)/uchardet/cmakebuild; \
	make -j$(NPROC); make install

$(DOWNLOADS)/uchardet/cmakebuild/Makefile: $(DOWNLOADS)/uchardet/CMakeLists.txt
	cd $(DOWNLOADS)/uchardet; \
	mkdir cmakebuild; cd cmakebuild; \
	$(CMAKE) -DBUILD_SHARED_LIBS=no

$(DOWNLOADS)/uchardet/CMakeLists.txt:
	$(CLONE) $(GITHUB)/mkxp-z/uchardet $(DOWNLOADS)/uchardet

# Sdl3
sdl3: init_dirs $(LIBDIR)/libSDL3.a

$(LIBDIR)/libSDL3.a: $(DOWNLOADS)/sdl3/cmakebuild/Makefile
	cd $(DOWNLOADS)/sdl3/cmakebuild; \
	make -j$(NPROC); make install

$(DOWNLOADS)/sdl3/cmakebuild/Makefile: $(DOWNLOADS)/sdl3/CMakeLists.txt
	cd $(DOWNLOADS)/sdl3; mkdir -p cmakebuild; cd cmakebuild; \
	$(CMAKE) -DSDL_STATIC=YES -DSDL_SHARED=NO -DSDL_OPENGLES=NO

$(DOWNLOADS)/sdl3/CMakeLists.txt:
	$(CLONE) --no-checkout $(GITHUB)/libsdl-org/SDL $(DOWNLOADS)/sdl3
	cd $(DOWNLOADS)/sdl3; git checkout release-3.2.0

# SDL_image
sdl3image: init_dirs sdl3 $(LIBDIR)/libSDL3_image.a

$(LIBDIR)/libSDL3_image.a: $(DOWNLOADS)/sdl3_image/cmakebuild/Makefile
	cd $(DOWNLOADS)/sdl3_image/cmakebuild; \
	make -j$(NPROC); make install

$(DOWNLOADS)/sdl3_image/cmakebuild/Makefile: $(DOWNLOADS)/sdl3_image/CMakeLists.txt
	cd $(DOWNLOADS)/sdl3_image; mkdir -p cmakebuild; cd cmakebuild; \
	$(CMAKE) \
	-DBUILD_SHARED_LIBS=no \
	-DSDL3IMAGE_JPG_SAVE=yes \
	-DSDL3IMAGE_PNG_SAVE=yes \
	-DSDL3IMAGE_PNG_SHARED=no \
	-DSDL3IMAGE_JPG_SHARED=no \
	-DSDL3IMAGE_JXL=no \
	-DSDL3IMAGE_JXL_SHARED=no \
	-DSDL3IMAGE_AVIF=no \
	-DSDL3IMAGE_VENDORED=yes

$(DOWNLOADS)/sdl3_image/CMakeLists.txt:
	$(CLONE) --no-checkout $(GITHUB)/libsdl-org/SDL_image $(DOWNLOADS)/sdl3_image
	cd $(DOWNLOADS)/sdl3_image; git checkout release-3.2.0; \
	./external/download.sh

# SDL3_sound
sdl3sound: init_dirs sdl3 libogg libvorbis $(LIBDIR)/libSDL3_sound.a

$(LIBDIR)/libSDL3_sound.a: $(DOWNLOADS)/sdl3_sound/cmakebuild/Makefile
	cd $(DOWNLOADS)/sdl3_sound/cmakebuild; \
	make -j$(NPROC); make install

$(DOWNLOADS)/sdl3_sound/cmakebuild/Makefile: $(DOWNLOADS)/sdl3_sound/CMakeLists.txt
	cd $(DOWNLOADS)/sdl3_sound; mkdir -p cmakebuild; cd cmakebuild; \
	$(CMAKE) \
	-DSDLSOUND_BUILD_SHARED=false \
	-DSDLSOUND_BUILD_TEST=false

$(DOWNLOADS)/sdl3_sound/CMakeLists.txt:
	$(CLONE) $(GITHUB)/icculus/SDL_sound $(DOWNLOADS)/sdl3_sound;
# SDL_sound hasn't made an official sdl3 release yet
	cd $(DOWNLOADS)/sdl3_sound; git checkout 9000c2b27a6aa210c5ceb5821deae396d67aa4cd;

# SDL3 (ttf)
sdl3ttf: init_dirs sdl3 $(LIBDIR)/libSDL3_ttf.a

$(LIBDIR)/libSDL3_ttf.a: $(DOWNLOADS)/sdl3_ttf/cmakebuild/Makefile
	cd $(DOWNLOADS)/sdl3_ttf/cmakebuild; \
	make -j$(NPROC); make install

$(DOWNLOADS)/sdl3_ttf/cmakebuild/Makefile: $(DOWNLOADS)/sdl3_ttf/CMakeLists.txt
# Plutosvg gets some weird errors on windows and I couldn't be bothered to understand why
	cd $(DOWNLOADS)/sdl3_ttf; mkdir cmakebuild; cd cmakebuild; \
	$(CMAKE) -DBUILD_SHARED_LIBS=no -DSDLTTF_VENDORED=ON -DSDLTTF_PLUTOSVG=OFF

$(DOWNLOADS)/sdl3_ttf/CMakeLists.txt:
	$(CLONE) $(GITHUB)/libsdl-org/SDL_ttf $(DOWNLOADS)/sdl3_ttf
# sdl3_ttf hasn't made an official sdl3 release yet
	cd $(DOWNLOADS)/sdl3_ttf; git checkout 485fd99baa7b954f41866f7afae3c6ef16bf943a; \
	./external/download.sh

# OpenAL
openal: init_dirs libogg $(LIBDIR)/libOpenAL32.a

$(LIBDIR)/libOpenAL32.a: $(DOWNLOADS)/openal/cmakebuild/Makefile
	cd $(DOWNLOADS)/openal/cmakebuild; \
	make -j$(NPROC); make install

$(DOWNLOADS)/openal/cmakebuild/Makefile: $(DOWNLOADS)/openal/CMakeLists.txt
	cd $(DOWNLOADS)/openal; mkdir cmakebuild; cd cmakebuild; \
	$(CMAKE) -DLIBTYPE=STATIC -DALSOFT_EXAMPLES=no -DALSOFT_UTILS=no $(OPENAL_FLAGS)

$(DOWNLOADS)/openal/CMakeLists.txt:
	$(CLONE) $(GITHUB)/mkxp-z/openal-soft $(DOWNLOADS)/openal

# FluidSynth
fluidsynth: init_dirs $(LIBDIR)/libfluidsynth.a

$(LIBDIR)/libfluidsynth.a: $(DOWNLOADS)/fluidsynth/cmakebuild/Makefile
	cd $(DOWNLOADS)/fluidsynth/cmakebuild; \
	make -j$(NPROC); make install

$(DOWNLOADS)/fluidsynth/cmakebuild/Makefile: $(DOWNLOADS)/fluidsynth/CMakeLists.txt
	cd $(DOWNLOADS)/fluidsynth; mkdir cmakebuild; cd cmakebuild; \
	$(CMAKE) -DBUILD_SHARED_LIBS=no -Denable-sdl2=no -Denable-readline=no

$(DOWNLOADS)/fluidsynth/CMakeLists.txt:
	$(CLONE) $(GITHUB)/mkxp-z/fluidsynth-sans-glib $(DOWNLOADS)/fluidsynth

# OpenSSL
openssl: init_dirs $(LIBDIR)/libssl.a
$(LIBDIR)/libssl.a: $(DOWNLOADS)/openssl/Makefile
	cd $(DOWNLOADS)/openssl; \
	make -j$(NPROC); make install_sw

$(DOWNLOADS)/openssl/Makefile: $(DOWNLOADS)/openssl/Configure
	cd $(DOWNLOADS)/openssl; \
	./Configure $(OPENSSL_FLAGS) \
	no-shared \
	--prefix="$(BUILD_PREFIX)" \
	--openssldir="$(BUILD_PREFIX)"

$(DOWNLOADS)/openssl/Configure:
	$(CLONE) $(GITHUB)/openssl/openssl $(DOWNLOADS)/openssl --single-branch --branch openssl-3.0.12 --depth 1

# Standard ruby
ruby: init_dirs openssl $(BINDIR)/$(RB_PREFIX)-ruby310.dll

$(BINDIR)/$(RB_PREFIX)-ruby310.dll: $(DOWNLOADS)/ruby/Makefile
	cd $(DOWNLOADS)/ruby; \
	make -j$(NPROC); make install

$(DOWNLOADS)/ruby/Makefile: $(DOWNLOADS)/ruby/configure
	cd $(DOWNLOADS)/ruby; \
	export $(CONFIGURE_ENV); \
	./configure $(CONFIGURE_ARGS) $(RUBY_CONFIGURE_ARGS) $(RUBY_FLAGS)

$(DOWNLOADS)/ruby/configure: $(DOWNLOADS)/ruby/configure.ac
	cd $(DOWNLOADS)/ruby; autoreconf -i

$(DOWNLOADS)/ruby/configure.ac:
	$(CLONE) $(GITHUB)/mkxp-z/ruby $(DOWNLOADS)/ruby --single-branch --branch mkxp-z-3.1.3 --depth 1;

libzmq: init_dirs $(LIBDIR)/libzmq.a

$(LIBDIR)/libzmq.a: $(DOWNLOADS)/libzmq/cmakebuild/Makefile
	cd $(DOWNLOADS)/libzmq/cmakebuild; \
	make -j$(NPROC); make install

$(DOWNLOADS)/libzmq/cmakebuild/Makefile: $(DOWNLOADS)/libzmq/CMakeLists.txt
	cd $(DOWNLOADS)/libzmq; mkdir -p cmakebuild; cd cmakebuild; \
	$(CMAKE) -DWITH_TLS=OFF -DWITH_LIBBSD=OFF -DZMQ_HAVE_IPC=off -DBUILD_SHARED=0

$(DOWNLOADS)/libzmq/CMakeLists.txt:
	$(CLONE) --branch v4.3.5 $(GITHUB)/zeromq/libzmq $(DOWNLOADS)/libzmq

cppzmq: init_dirs libzmq $(INCLUDEDIR)/zmq.hpp

$(INCLUDEDIR)/zmq.hpp: $(DOWNLOADS)/cppzmq/cmakebuild/Makefile
	cd $(DOWNLOADS)/cppzmq/cmakebuild; \
	make -j$(NPROC); make install

$(DOWNLOADS)/cppzmq/cmakebuild/Makefile: $(DOWNLOADS)/cppzmq/CMakeLists.txt
	cd $(DOWNLOADS)/cppzmq; mkdir -p cmakebuild; cd cmakebuild; \
	$(CMAKE) -DCPPZMQ_BUILD_TESTS=off

$(DOWNLOADS)/cppzmq/CMakeLists.txt:
	$(CLONE) --branch v4.10.0 $(GITHUB)/zeromq/cppzmq $(DOWNLOADS)/cppzmq

# ====
init_dirs:
	@mkdir -p $(LIBDIR) $(INCLUDEDIR)

powerwash: clean-downloads

clean-downloads:
	-rm -rf downloads

deps-core: libtheora libvorbis pixman libpng physfs sdl3 sdl3image sdl3sound sdl3ttf openal openssl fluidsynth uchardet cppzmq
everything: deps-core ruby
