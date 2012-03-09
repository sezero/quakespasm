### GNU Makefile for QuakeSpasm unix targets, March 09, 2012
#
# You need the SDL library fully installed.
# "make DEBUG=1" to build a debug client.
# "make SDL_CONFIG=/path/to/sdl-config" for unusual SDL installations.
# "make SDLNET=1" to use SDL_net (NOT recommended) instead of platform
# specific code.

### Embed custom Quakespasm console background picture
USE_QS_CONBACK=1

### Enable/Disable codecs for streaming music support
USE_CODEC_WAVE=1
USE_CODEC_MP3=1
USE_CODEC_VORBIS=1

# which library to use for mp3 decoding: mad or mpg123
MP3LIB=mad
# which library to use for ogg decoding: vorbis or tremor
VORBISLIB=vorbis

# ============================================================================
# Helper functions
# ============================================================================

check_gcc = $(shell if echo | $(CC) $(1) -S -o /dev/null -xc - > /dev/null 2>&1; then echo "$(1)"; else echo "$(2)"; fi;)

# ============================================================================

DEBUG    ?= 0
SDLNET   ?= 0

BUILDDIR := .

SYSNAME := $(shell uname -s)

ifneq (,$(findstring MINGW32,$(SYSNAME)))
HOST_OS = WIN32
TOPDIR := $(shell pwd -W)
else
ifneq (,$(findstring $(SYSNAME),FreeBSD NetBSD OpenBSD))
HOST_OS = UNIX
UNIX = bsd
TOPDIR := $(shell pwd)
else
ifneq (,$(findstring Linux,$(SYSNAME)))
HOST_OS = UNIX
UNIX = linux
TOPDIR := $(shell pwd)
else
$(error OS type not detected.)
endif
endif
endif

# ---------------------------------------
# build variables
# ---------------------------------------

CC ?= gcc
WINDRES ?= windres
STRIP ?= strip

#CPUFLAGS= -mtune=i686
#CPUFLAGS= -march=pentium4
#CPUFLAGS= -mtune=k8
#CPUFLAGS= -march=atom
CPUFLAGS= 
DFLAGS ?=
CFLAGS ?= -Wall -Wno-trigraphs
# CFLAGS += -Werror
CFLAGS += $(CPUFLAGS)

ifneq ($(DEBUG),0)
DFLAGS += -DDEBUG
CFLAGS += -g
do_strip=
else
DFLAGS += -DNDEBUG
CFLAGS += -O2
CFLAGS += $(call check_gcc,-fweb,)
CFLAGS += $(call check_gcc,-frename-registers,)
cmd_strip=$(STRIP) $(1)
define do_strip
	$(call cmd_strip,$(1));
endef
endif

### X11BASE only gets used if its in an unusual place

X11DIRS := /usr/X11R7 /usr/local/X11R7 /usr/X11R6 /usr/local/X11R6
X11BASE_GUESS := $(shell \
	if [ -e /usr/include/X11/Xlib.h ] && \
		[ -e /usr/lib/libX11.a ]; then exit 0; fi; \
	if [ -e /usr/local/include/X11/Xlib.h ] && \
		[ -e /usr/local/lib/libX11.a ]; then exit 0; fi; \
	for DIR in $(X11DIRS); do \
            if [ -e $$DIR/include/X11/Xlib.h ] && \
		[ -e $$DIR/lib/libX11.a ]; then echo $$DIR; break; fi; \
	done )

X11BASE	?= $(X11BASE_GUESS)

ifneq ($(X11BASE),)
X11_LFLAGS := -L$(X11BASE)/lib
X11_CFLAGS := -I$(X11BASE)/include
endif

SDL_CONFIG ?= sdl-config
SDL_CFLAGS := $(shell $(SDL_CONFIG) --cflags)
SDL_LIBS   := $(shell $(SDL_CONFIG) --libs)

ifeq ($(SDLNET),1)
NET_LIBS   :=-lSDL_net
CFLAGS     +=-D_USE_SDLNET
else
NET_LIBS   :=
endif

ifeq ($(USE_QS_CONBACK),1)
CFLAGS+= -DUSE_QS_CONBACK
endif

ifneq ($(VORBISLIB),vorbis)
ifneq ($(VORBISLIB),tremor)
$(error Invalid VORBISLIB setting)
endif
endif
ifneq ($(MP3LIB),mpg123)
ifneq ($(MP3LIB),mad)
$(error Invalid MP3LIB setting)
endif
endif
ifeq ($(MP3LIB),mad)
mp3_obj=snd_mp3.o
lib_mp3dec=-lmad
endif
ifeq ($(MP3LIB),mpg123)
mp3_obj=snd_mpg123.o
lib_mp3dec=-lmpg123
endif
ifeq ($(VORBISLIB),vorbis)
cpp_vorbisdec=
lib_vorbisdec=-lvorbisfile -lvorbis -logg
endif
ifeq ($(VORBISLIB),tremor)
cpp_vorbisdec=-DVORBIS_USE_TREMOR
lib_vorbisdec=-lvorbisidec -logg
endif

CODECLIBS  :=
ifeq ($(USE_CODEC_WAVE),1)
CFLAGS+= -DUSE_CODEC_WAVE
endif
ifeq ($(USE_CODEC_VORBIS),1)
CFLAGS+= -DUSE_CODEC_VORBIS $(cpp_vorbisdec)
CODECLIBS+= $(lib_vorbisdec)
endif
ifeq ($(USE_CODEC_MP3),1)
CFLAGS+= -DUSE_CODEC_MP3
CODECLIBS+= $(lib_mp3dec)
endif

COMMON_LIBS:= -lm -lGL

LIBS := $(COMMON_LIBS) $(NET_LIBS) $(CODECLIBS)

# ---------------------------
# targets
# ---------------------------

.PHONY:	clean debug release

DEFAULT_TARGET := quakespasm

# ---------------------------
# rules
# ---------------------------

%.o:	%.c
	$(CC) $(DFLAGS) -c $(CFLAGS) $(X11_CFLAGS) $(SDL_CFLAGS) -o $@ $^

# ----------------------------------------------------------------------------
# objects
# ----------------------------------------------------------------------------

MUSIC_OBJS:= bgmusic.o \
	snd_codec.o \
	snd_wave.o \
	snd_vorbis.o \
	$(mp3_obj)
COMOBJ_SND := snd_dma.o snd_mix.o snd_mem.o $(MUSIC_OBJS)
SYSOBJ_SND := snd_sdl.o
SYSOBJ_CDA := cd_sdl.o
SYSOBJ_INPUT := in_sdl.o
SYSOBJ_GL_VID:= gl_vidsdl.o
ifeq ($(SDLNET),1)
SYSOBJ_NET := net_sdl.o net_sdlnet.o
else
SYSOBJ_NET := net_bsd.o net_udp.o
endif
SYSOBJ_SYS := pl_linux.o sys_sdl_unix.o
SYSOBJ_MAIN:= main_sdl.o
SYSOBJ_RES :=

GLOBJS = \
	gl_refrag.o \
	gl_rlight.o \
	gl_rmain.o \
	gl_fog.o \
	gl_rmisc.o \
	r_part.o \
	r_world.o \
	gl_screen.o \
	gl_sky.o \
	gl_warp.o \
	$(SYSOBJ_GL_VID) \
	gl_draw.o \
	image.o \
	gl_texmgr.o \
	gl_mesh.o \
	r_sprite.o \
	r_alias.o \
	r_brush.o \
	gl_model.o

OBJS := strlcat.o \
	strlcpy.o \
	$(GLOBJS) \
	$(SYSOBJ_INPUT) \
	$(COMOBJ_SND) \
	$(SYSOBJ_SND) \
	$(SYSOBJ_CDA) \
	$(SYSOBJ_NET) \
	net_dgrm.o \
	net_loop.o \
	net_main.o \
	chase.o \
	cl_demo.o \
	cl_input.o \
	cl_main.o \
	cl_parse.o \
	cl_tent.o \
	console.o \
	keys.o \
	menu.o \
	sbar.o \
	view.o \
	wad.o \
	cmd.o \
	common.o \
	crc.o \
	cvar.o \
	cfgfile.o \
	host.o \
	host_cmd.o \
	mathlib.o \
	pr_cmds.o \
	pr_edict.o \
	pr_exec.o \
	sv_main.o \
	sv_move.o \
	sv_phys.o \
	sv_user.o \
	world.o \
	conback.o \
	zone.o \
	$(SYSOBJ_SYS) $(SYSOBJ_MAIN) $(SYSOBJ_RES)

# ------------------------
# Linux build rules
# ------------------------

quakespasm:	$(OBJS)
	$(CC) $(OBJS) $(X11_LFLAGS) $(LIBS) $(SDL_LIBS) -o $@
	$(call do_strip,$@)

release:	quakespasm
debug:
	$(error Use "make DEBUG=1")

install:	quakespasm
	cp quakespasm /usr/local/games/quake/

clean:
	rm -f $(shell find . \( -name '*~' -o -name '#*#' -o -name '*.o' -o -name '*.res' -o -name $(DEFAULT_TARGET) \) -print)

