ARCH ?= $(shell uname -m)
ARCH_OPENSSL ?= linux-$(ARCH)
ARCH_CFLAGS ?=
ARCH_CONFIGURE ?= $(shell gcc -dumpmachine)

SDL_FLAGS := ${SDL_FLAGS}
SDL3_IMAGE_FLAGS := ${SDL3_IMAGE_FLAGS}
SDL3_TTF_FLAGS := ${SDL3_TTF_FLAGS}
OPENAL_FLAGS := -DALSOFT_CPUEXT_NEON=no ${OPENAL_FLAGS}
OPENSSL_FLAGS := $(ARCH_OPENSSL) ${OPENSSL_FLAGS}
RUBY_FLAGS := ${RUBY_FLAGS}

BUILD_PREFIX := ${PWD}/build-$(ARCH)
LIBDIR := $(BUILD_PREFIX)/lib
INCLUDEDIR := $(BUILD_PREFIX)/include
DOWNLOADS := ${PWD}/downloads/$(ARCH)
NPROC := $(shell nproc)
CFLAGS := -I$(INCLUDEDIR) -flax-vector-conversions -O3 $(ARCH_CFLAGS)
LDFLAGS := -L$(LIBDIR)
CC      ?= gcc
PKG_CONFIG_LIBDIR := $(BUILD_PREFIX)/lib/pkgconfig
GIT := git
CLONE := $(GIT) clone -q
GITHUB := https://github.com

CONFIGURE_ENV := \
	CMAKE_POLICY_VERSION_MINIMUM=3.10 \
	PKG_CONFIG_LIBDIR=$(PKG_CONFIG_LIBDIR) \
	CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"

CONFIGURE_ARGS := --prefix="$(BUILD_PREFIX)" --host="$(ARCH_CONFIGURE)"

ifdef ARCH_CMAKE_TOOLCHAIN
	ARCH_CMAKE_ARGS := -DCMAKE_TOOLCHAIN_FILE="${PWD}/$(ARCH_CMAKE_TOOLCHAIN)"
endif
CMAKE_ARGS := \
	-DCMAKE_INSTALL_PREFIX="$(BUILD_PREFIX)" \
	-DCMAKE_PREFIX_PATH="$(BUILD_PREFIX)" \
	-DCMAKE_C_FLAGS="$(CFLAGS)" \
	-DCMAKE_BUILD_TYPE=Release \
	$(ARCH_CMAKE_ARGS)

# TODO: Check if we can disable rubygems, remove prefixes.
RUBY_CONFIGURE_ARGS := \
	--enable-shared \
	--disable-install-doc
#	--enable-install-static-library \
#	--with-out-ext=openssl,readline,dbm,gdbm \
#	--with-static-linked-ext \
#	--disable-rubygems \
#	--without-gmp

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


# iconv
iconv: init_dirs $(LIBDIR)/libiconv.a

$(LIBDIR)/libiconv.a: $(DOWNLOADS)/libiconv-1.18/Makefile
	cd $(DOWNLOADS)/libiconv-1.18; make; make install

$(DOWNLOADS)/libiconv-1.18/Makefile: $(DOWNLOADS)/libiconv-1.18/configure
	cd $(DOWNLOADS)/libiconv-1.18; \
	$(CONFIGURE) --enable-static=true --enable-shared=false

$(DOWNLOADS)/libiconv-1.18/configure: $(DOWNLOADS)/libiconv-1.18.tar.gz
	cd $(DOWNLOADS); \
	tar -xzf libiconv-1.18.tar.gz;

$(DOWNLOADS)/libiconv-1.18.tar.gz:
	wget https://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.18.tar.gz -P $(DOWNLOADS)

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

# Sdl3
sdl3: init_dirs $(LIBDIR)/libSDL3.a

$(LIBDIR)/libSDL3.a: $(DOWNLOADS)/sdl3/cmakebuild/Makefile
	cd $(DOWNLOADS)/sdl3/cmakebuild; \
	$(MAKE) -j$(NPROC); $(MAKE) install

$(DOWNLOADS)/sdl3/cmakebuild/Makefile: $(DOWNLOADS)/sdl3/CMakeLists.txt
	cd $(DOWNLOADS)/sdl3; mkdir -p cmakebuild; cd cmakebuild; \
	$(CMAKE) -DSDL_STATIC=1 -DSDL_SHARED=0

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
# AVIF building fails on my machine (no idea)
# FIXME: one of these requires nasm!

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
	cd $(DOWNLOADS)/sdl3_sound; git checkout f6a186bb5b2f6543b052afdace769f84322ea151;

# SDL3 (ttf)
sdl3ttf: init_dirs sdl3 $(LIBDIR)/libSDL3_ttf.a

$(LIBDIR)/libSDL3_ttf.a: $(DOWNLOADS)/sdl3_ttf/cmakebuild/Makefile
	cd $(DOWNLOADS)/sdl3_ttf/cmakebuild; \
	make -j$(NPROC); make install

$(DOWNLOADS)/sdl3_ttf/cmakebuild/Makefile: $(DOWNLOADS)/sdl3_ttf/CMakeLists.txt
	cd $(DOWNLOADS)/sdl3_ttf; mkdir cmakebuild; cd cmakebuild; \
	$(CMAKE) -DBUILD_SHARED_LIBS=no -DSDLTTF_VENDORED=ON

$(DOWNLOADS)/sdl3_ttf/CMakeLists.txt:
	$(CLONE) $(GITHUB)/libsdl-org/SDL_ttf $(DOWNLOADS)/sdl3_ttf
# sdl3_ttf hasn't made an official sdl3 release yet
	cd $(DOWNLOADS)/sdl3_ttf; git checkout 83c00a0d9a9542887295928753897a50ef8541e0; \
	./external/download.sh

# OpenAL
openal: init_dirs libogg $(LIBDIR)/libopenal.a

$(LIBDIR)/libopenal.a: $(DOWNLOADS)/openal/cmakebuild/Makefile
	cd $(DOWNLOADS)/openal/cmakebuild; \
	make -j$(NPROC); make install

$(DOWNLOADS)/openal/cmakebuild/Makefile: $(DOWNLOADS)/openal/CMakeLists.txt
	cd $(DOWNLOADS)/openal; mkdir cmakebuild; cd cmakebuild; \
	$(CMAKE) -DLIBTYPE=STATIC -DALSOFT_EXAMPLES=no -DALSOFT_UTILS=no $(OPENAL_FLAGS)

$(DOWNLOADS)/openal/CMakeLists.txt:
	$(CLONE) $(GITHUB)/kcat/openal-soft $(DOWNLOADS)/openal --branch 1.24.3

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
ruby: init_dirs openssl $(LIBDIR)/libruby.so.3.4

$(LIBDIR)/libruby.so.3.4: $(DOWNLOADS)/ruby/Makefile
	cd $(DOWNLOADS)/ruby; \
	make -j$(NPROC); make install DESTDIR="$(BUILD_PREFIX)"

$(DOWNLOADS)/ruby/Makefile: $(DOWNLOADS)/ruby/configure
	cd $(DOWNLOADS)/ruby; \
	export $(CONFIGURE_ENV); \
	./configure $(CONFIGURE_ARGS) $(RUBY_CONFIGURE_ARGS) $(RUBY_FLAGS)

$(DOWNLOADS)/ruby/configure: $(DOWNLOADS)/ruby/configure.ac
	cd $(DOWNLOADS)/ruby; autoreconf -i

$(DOWNLOADS)/ruby/configure.ac:
	$(CLONE) $(GITHUB)/ruby/ruby $(DOWNLOADS)/ruby --single-branch --branch v3_4_4 --depth 1;

# ====
init_dirs:
	@mkdir -p $(LIBDIR) $(INCLUDEDIR)

powerwash: clean-downloads

clean-downloads:
	-rm -rf downloads

deps-core: libtheora libvorbis pixman libpng physfs sdl3 sdl3image sdl3sound sdl3ttf openal openssl uchardet iconv
everything: deps-core ruby
