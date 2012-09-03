#!/bin/sh

# Change this script to meet your needs and/or environment.

#TARGET=x86_64-pc-mingw32
TARGET=x86_64-w64-mingw32
PREFIX=/opt/cross_win64

PATH="$PREFIX/bin:$PATH"
export PATH

MAKE_CMD=make

SDL_CONFIG=/opt/sdl_w64/bin/sdl-config
CC="$TARGET-gcc"
AS="$TARGET-as"
RANLIB="$TARGET-ranlib"
AR="$TARGET-ar"
WINDRES="$TARGET-windres"
STRIP="$TARGET-strip"
export PATH CC AS AR RANLIB WINDRES STRIP

exec $MAKE_CMD SDL_CONFIG=$SDL_CONFIG CC=$CC AS=$AS RANLIB=$RANLIB AR=$AR WINDRES=$WINDRES STRIP=$STRIP -f Makefile.w64 $*
