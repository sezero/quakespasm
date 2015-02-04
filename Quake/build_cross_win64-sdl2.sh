#!/bin/sh

# Change this script to meet your needs and/or environment.

TARGET=x86_64-w64-mingw32
PREFIX=/opt/cross_win64

PATH="$PREFIX/bin:$PATH"
export PATH

MAKE_CMD=make

SDL_CONFIG=/opt/sdl2-w64/bin/sdl2-config
CC="$TARGET-gcc"
AS="$TARGET-as"
RANLIB="$TARGET-ranlib"
AR="$TARGET-ar"
WINDRES="$TARGET-windres"
STRIP="$TARGET-strip"
export PATH CC AS AR RANLIB WINDRES STRIP

exec $MAKE_CMD USE_SDL2=1 SDL_CONFIG=$SDL_CONFIG CC=$CC AS=$AS RANLIB=$RANLIB AR=$AR WINDRES=$WINDRES STRIP=$STRIP -f Makefile.w64 $*
