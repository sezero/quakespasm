# GNU Makefile for QuakeSpasm unix targets, Aug. 31, 2010
#
# You need the SDL library fully installed.
# "make DEBUG=1" to build a debug client.
# "make SDL_CONFIG=/path/to/sdl-config" for unusual SDL installations.
# "make SDLNET=1" to use SDL_net (not recommended) instead of platform
# specific code.
#
# Build objects are separate from those of codeblocks for now
# (we aren't using codeblocks).

# ============================================================================

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
# Define some build variables
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
SDL_LFLAGS := $(shell $(SDL_CONFIG) --libs)

ifeq ($(SDLNET),1)
NET_LIBS   := SDL_net
CFLAGS     +=-D_USE_SDLNET
else
NET_LIBS   :=
endif

COMMON_LIBS:= m GL

LIBS := $(patsubst %,-l%,$(COMMON_LIBS) $(NET_LIBS))

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

COMOBJ_SND := snd_dma.o snd_mix.o snd_mem.o
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

OBJS := \
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
# build rules for Linux
# ------------------------

quakespasm:	$(OBJS)
	$(CC) $(CFLAGS) -o quakespasm $(OBJS) $(X11_LFLAGS) $(SDL_LFLAGS) $(LIBS)
	$(call do_strip,$@)

release:	quakespasm
debug:
	$(error Use "make DEBUG=1")

install:	quakespasm
	cp quakespasm /usr/local/games/quake

clean:
	rm -f $(shell find . \( -name '*~' -o -name '#*#' -o -name '*.o' -o -name '*.res' -o -name $(DEFAULT_TARGET) \) -print)

